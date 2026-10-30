#include "krunnerclient.h"

#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QtConcurrent>

#include <algorithm>

namespace kestrel {
namespace {

constexpr auto kInterfaceName = "org.kde.krunner1";
constexpr int kQueryDebounceMs = 80;
constexpr int kRunnerTimeoutMs = 120;
constexpr int kMaximumRunnerResults = 20;

QVector<KRunnerDescriptor> scanRunnerDescriptors(const QStringList &directories)
{
    QVector<KRunnerDescriptor> descriptors;
    QSet<QString> seen;
    const QStringList filters{QStringLiteral("*.desktop")};

    for (const QString &directory : directories) {
        QDirIterator iterator(directory, filters, QDir::Files, QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const KRunnerDescriptor descriptor = parseKRunnerDescriptor(iterator.next());
            const QString key = descriptor.service + QLatin1Char('|') + descriptor.objectPath;
            if (!descriptor.service.isEmpty() && !seen.contains(key)) {
                seen.insert(key);
                descriptors.append(descriptor);
            }
        }
    }
    return descriptors;
}

QStringList runnerDataDirectories()
{
    QStringList directories;
    const QStringList dataLocations =
        QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    const QStringList relativePaths{QStringLiteral("kservices6"), QStringLiteral("kservices5"),
                                    QStringLiteral("plasma/krunner")};

    for (const QString &location : dataLocations) {
        for (const QString &relativePath : relativePaths) {
            const QString directory = QDir(location).filePath(relativePath);
            if (QFileInfo(directory).isDir()) {
                directories.append(directory);
            }
        }
    }
    return directories;
}

} // namespace

QDBusArgument &operator<<(QDBusArgument &argument, const KRunnerRemoteMatch &match)
{
    argument.beginStructure();
    argument << match.id << match.text << match.iconName << match.categoryRelevance
             << match.relevance << match.properties;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, KRunnerRemoteMatch &match)
{
    argument.beginStructure();
    argument >> match.id >> match.text >> match.iconName >> match.categoryRelevance
        >> match.relevance >> match.properties;
    argument.endStructure();
    return argument;
}

KRunnerDescriptor parseKRunnerDescriptor(const QString &path)
{
    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Desktop Entry"));
    const QString api = settings.value(QStringLiteral("X-Plasma-API")).toString();
    const QString service = settings.value(QStringLiteral("X-Plasma-DBusRunner-Service")).toString();
    const QString objectPath = settings.value(QStringLiteral("X-Plasma-DBusRunner-Path"),
                                              QStringLiteral("/runner")).toString();
    const bool enabled = settings.value(QStringLiteral("X-KDE-PluginInfo-EnabledByDefault"), true)
                             .toBool();
    settings.endGroup();

    if ((api != QLatin1String("DBus") && api != QLatin1String("DBus2")) || !enabled
        || service.isEmpty() || objectPath.isEmpty()) {
        return {};
    }
    return {service, objectPath};
}

KRunnerClient::KRunnerClient(QObject *parent)
    : QObject(parent)
    , m_bus(QDBusConnection::sessionBus())
{
    qDBusRegisterMetaType<KRunnerRemoteMatch>();
    qDBusRegisterMetaType<KRunnerRemoteMatches>();

    QTimer::connect(&m_discoveryWatcher, &QFutureWatcherBase::finished, this, [this] {
        m_runners = m_discoveryWatcher.result();
        m_discoveryFinished = true;
        if (m_query.trimmed().size() > 0) {
            dispatchSearch();
        }
    });

    if (m_bus.interface()) {
        connect(m_bus.interface(), &QDBusConnectionInterface::serviceOwnerChanged, this,
                [this](const QString &service, const QString &, const QString &newOwner) {
                    if (newOwner.isEmpty()) {
                        m_registeredServices.remove(service);
                    } else {
                        m_registeredServices.insert(service);
                    }
                });

        const QDBusMessage listNames = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
            QStringLiteral("org.freedesktop.DBus"), QStringLiteral("ListNames"));
        auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(listNames), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this,
                [this, watcher](QDBusPendingCallWatcher *finished) {
                    const QDBusPendingReply<QStringList> reply = *finished;
                    finished->deleteLater();
                    if (reply.isError()) {
                        qWarning().noquote() << "Could not list session D-Bus services:"
                                             << reply.error().message();
                        return;
                    }
                    for (const QString &service : reply.value()) {
                        m_registeredServices.insert(service);
                    }
                });
    }

    QTimer::connect(&m_queryTimer, &QTimer::timeout, this, &KRunnerClient::dispatchSearch);
    m_queryTimer.setSingleShot(true);
    m_queryTimer.setInterval(kQueryDebounceMs);
    startDiscovery();
}

void KRunnerClient::startDiscovery()
{
    const QStringList directories = runnerDataDirectories();
    m_discoveryWatcher.setFuture(QtConcurrent::run(scanRunnerDescriptors, directories));
}

void KRunnerClient::search(const QString &query)
{
    m_query = query.trimmed();
    ++m_generation;
    m_queryTimer.stop();
    m_pendingMatches.clear();
    m_pendingReplies = 0;

    if (m_query.isEmpty()) {
        emit matchesReady(m_query, {});
        return;
    }
    if (m_discoveryFinished) {
        m_queryTimer.start();
    }
}

void KRunnerClient::dispatchSearch()
{
    if (m_query.isEmpty() || !m_discoveryFinished || !m_bus.isConnected()) {
        return;
    }

    const quint64 generation = m_generation;
    QSet<QString> requested;
    for (const KRunnerDescriptor &runner : std::as_const(m_runners)) {
        for (const QString &service : servicesForRunner(runner)) {
            const QString key = service + QLatin1Char('|') + runner.objectPath;
            if (requested.contains(key)) {
                continue;
            }
            requested.insert(key);

            QDBusMessage message = QDBusMessage::createMethodCall(
                service, runner.objectPath, QString::fromLatin1(kInterfaceName),
                QStringLiteral("Match"));
            message.setArguments({m_query});

            ++m_pendingReplies;
            auto *watcher =
                new QDBusPendingCallWatcher(m_bus.asyncCall(message, kRunnerTimeoutMs), this);
            connect(watcher, &QDBusPendingCallWatcher::finished, this,
                    [this, watcher, service, objectPath = runner.objectPath, generation](
                        QDBusPendingCallWatcher *finished) {
                        const QDBusPendingReply<KRunnerRemoteMatches> reply = *finished;
                        finished->deleteLater();
                        if (generation != m_generation) {
                            return;
                        }
                        if (reply.isError()) {
                            qWarning().noquote() << "KRunner query failed for" << service << ":"
                                                 << reply.error().message();
                        } else {
                            for (const KRunnerRemoteMatch &remote : reply.value()) {
                                KRunnerMatch match;
                                match.id = remote.id;
                                match.text = remote.text;
                                match.iconName = remote.iconName;
                                match.subtitle =
                                    remote.properties.value(QStringLiteral("subtext")).toString();
                                match.category =
                                    remote.properties.value(QStringLiteral("category")).toString();
                                match.service = service;
                                match.objectPath = objectPath;
                                match.categoryRelevance = remote.categoryRelevance;
                                match.relevance = remote.relevance;
                                m_pendingMatches.append(match);
                            }
                        }
                        --m_pendingReplies;
                        if (m_pendingReplies == 0) {
                            finishSearch();
                        }
                    });
        }
    }

    if (m_pendingReplies == 0) {
        finishSearch();
    }
}

void KRunnerClient::finishSearch()
{
    std::stable_sort(m_pendingMatches.begin(), m_pendingMatches.end(),
                     [](const KRunnerMatch &left, const KRunnerMatch &right) {
                         if (left.categoryRelevance != right.categoryRelevance) {
                             return left.categoryRelevance > right.categoryRelevance;
                         }
                         return left.relevance > right.relevance;
                     });
    if (m_pendingMatches.size() > kMaximumRunnerResults) {
        m_pendingMatches.resize(kMaximumRunnerResults);
    }
    emit matchesReady(m_query, m_pendingMatches);
}

QStringList KRunnerClient::servicesForRunner(const KRunnerDescriptor &runner) const
{
    if (!runner.service.endsWith(QLatin1Char('*'))) {
        return {runner.service};
    }

    const QString prefix = runner.service.chopped(1);
    QStringList services;
    for (const QString &registered : m_registeredServices) {
        if (registered.startsWith(prefix)) {
            services.append(registered);
        }
    }
    return services;
}

void KRunnerClient::run(const KRunnerMatch &match)
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        match.service, match.objectPath, QString::fromLatin1(kInterfaceName),
        QStringLiteral("Run"));
    message.setArguments({match.id, QString()});

    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [](QDBusPendingCallWatcher *finished) {
                const QDBusPendingReply<> reply = *finished;
                if (reply.isError()) {
                    qWarning().noquote() << "KRunner action failed:" << reply.error().message();
                }
                finished->deleteLater();
            });
}

} // namespace kestrel
