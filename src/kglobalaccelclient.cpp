#include "kglobalaccelclient.h"

#include <QCoreApplication>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QKeySequence>
#include <QVariantMap>

namespace kestrel {
namespace {

constexpr auto kServiceName = "org.kde.kglobalaccel";
constexpr auto kObjectPath = "/kglobalaccel";
constexpr auto kInterfaceName = "org.kde.KGlobalAccel";
constexpr auto kComponentInterfaceName = "org.kde.kglobalaccel.Component";
constexpr auto kComponentUniqueName = "Kestrel";
constexpr auto kActionUniqueName = "togglePalette";
constexpr uint kSetPresentFlag = 1U << 1;
constexpr uint kIsDefaultFlag = 1U << 3;

QKeySequence defaultShortcut()
{
    return QKeySequence(Qt::META | Qt::Key_Space);
}

} // namespace

KGlobalAccelClient::KGlobalAccelClient(QObject *parent)
    : QObject(parent)
    , m_bus(QDBusConnection::sessionBus())
    , m_accelInterface(QString::fromLatin1(kServiceName), QString::fromLatin1(kObjectPath),
                       QString::fromLatin1(kInterfaceName), m_bus)
    , m_actionId{QString::fromLatin1(kComponentUniqueName), QString::fromLatin1(kActionUniqueName),
                 QCoreApplication::applicationName(),
                 QCoreApplication::translate("KGlobalAccelClient", "Toggle Kestrel")}
{
}

KGlobalAccelClient::~KGlobalAccelClient()
{
    if (m_registered && m_bus.isConnected()) {
        m_accelInterface.asyncCall(QStringLiteral("setInactive"), m_actionId);
    }
}

void KGlobalAccelClient::registerToggleShortcut()
{
    if (m_registrationStarted) {
        return;
    }
    m_registrationStarted = true;

    if (!m_bus.isConnected()) {
        qWarning("KGlobalAccel unavailable: the session D-Bus is not connected");
        return;
    }

    auto *watcher = new QDBusPendingCallWatcher(
        m_accelInterface.asyncCall(QStringLiteral("doRegister"), m_actionId), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher](QDBusPendingCallWatcher *finished) {
                const QDBusPendingReply<> reply = *finished;
                finished->deleteLater();
                if (reply.isError()) {
                    qWarning().noquote() << "KGlobalAccel could not register Kestrel:"
                                         << reply.error().message();
                    return;
                }

                m_registered = true;
                registerComponent();
            });
}

void KGlobalAccelClient::registerComponent()
{
    auto *watcher = new QDBusPendingCallWatcher(
        m_accelInterface.asyncCall(QStringLiteral("getComponent"),
                                   QString::fromLatin1(kComponentUniqueName)),
        this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *finished) {
                const QDBusPendingReply<QDBusObjectPath> reply = *finished;
                finished->deleteLater();
                if (reply.isError()) {
                    qWarning().noquote() << "KGlobalAccel could not resolve Kestrel's component:"
                                         << reply.error().message();
                    return;
                }

                connectComponent(reply.value().path());
            });
}

void KGlobalAccelClient::connectComponent(const QString &objectPath)
{
    const bool connected = m_bus.connect(
        QString::fromLatin1(kServiceName), objectPath,
        QString::fromLatin1(kComponentInterfaceName), QStringLiteral("pressed"), this,
        SLOT(handleShortcutPressed(QString,QVariantMap)));
    if (!connected) {
        qWarning("KGlobalAccel signal connection failed; the global shortcut is inactive");
        m_accelInterface.asyncCall(QStringLiteral("setInactive"), m_actionId);
        m_registered = false;
        return;
    }

    setActiveShortcut();
}

void KGlobalAccelClient::setActiveShortcut()
{
    const QList<int> key = {defaultShortcut()[0].toCombined()};
    auto *watcher = new QDBusPendingCallWatcher(
        m_accelInterface.asyncCallWithArgumentList(
            QStringLiteral("setShortcut"),
            {QVariant::fromValue(m_actionId), QVariant::fromValue(key),
             QVariant::fromValue(kSetPresentFlag)}),
        this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *finished) {
                const QDBusPendingReply<QList<int>> reply = *finished;
                finished->deleteLater();
                if (reply.isError()) {
                    qWarning().noquote() << "KGlobalAccel could not set Kestrel's active shortcut:"
                                         << reply.error().message();
                    return;
                }
                if (reply.value().isEmpty()) {
                    qWarning("Kestrel's default shortcut is unavailable; configure it in "
                             "System Settings > Shortcuts");
                }

                setDefaultShortcut();
            });
}

void KGlobalAccelClient::setDefaultShortcut()
{
    const QList<int> key = {defaultShortcut()[0].toCombined()};
    auto *watcher = new QDBusPendingCallWatcher(
        m_accelInterface.asyncCallWithArgumentList(
            QStringLiteral("setShortcut"),
            {QVariant::fromValue(m_actionId), QVariant::fromValue(key),
             QVariant::fromValue(kIsDefaultFlag)}),
        this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [](QDBusPendingCallWatcher *finished) {
                const QDBusPendingReply<QList<int>> reply = *finished;
                if (reply.isError()) {
                    qWarning().noquote() << "KGlobalAccel could not save Kestrel's default shortcut:"
                                         << reply.error().message();
                }
                finished->deleteLater();
            });
}

void KGlobalAccelClient::handleShortcutPressed(const QString &actionId,
                                               const QVariantMap &options)
{
    Q_UNUSED(options);
    if (actionId == QString::fromLatin1(kActionUniqueName)) {
        emit toggleRequested();
    }
}

} // namespace kestrel
