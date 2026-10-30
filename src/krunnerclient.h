#pragma once

#include <QDBusConnection>
#include <QDBusArgument>
#include <QFutureWatcher>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVariantMap>
#include <QVector>

namespace kestrel {

struct KRunnerDescriptor
{
    QString service;
    QString objectPath;
};

struct KRunnerMatch
{
    QString id;
    QString text;
    QString iconName;
    QString subtitle;
    QString category;
    QString service;
    QString objectPath;
    int categoryRelevance = 0;
    double relevance = 0.0;
};

struct KRunnerRemoteMatch
{
    QString id;
    QString text;
    QString iconName;
    int categoryRelevance = 0;
    double relevance = 0.0;
    QVariantMap properties;
};

using KRunnerRemoteMatches = QList<KRunnerRemoteMatch>;

KRunnerDescriptor parseKRunnerDescriptor(const QString &path);

QDBusArgument &operator<<(QDBusArgument &argument, const KRunnerRemoteMatch &match);
const QDBusArgument &operator>>(const QDBusArgument &argument, KRunnerRemoteMatch &match);

class KRunnerClient : public QObject
{
    Q_OBJECT

public:
    explicit KRunnerClient(QObject *parent = nullptr);

    void search(const QString &query);
    void run(const KRunnerMatch &match);

signals:
    void matchesReady(const QString &query, const QVector<kestrel::KRunnerMatch> &matches);

private:
    void startDiscovery();
    void dispatchSearch();
    void finishSearch();
    QStringList servicesForRunner(const KRunnerDescriptor &runner) const;

    QDBusConnection m_bus;
    QFutureWatcher<QVector<KRunnerDescriptor>> m_discoveryWatcher;
    QVector<KRunnerDescriptor> m_runners;
    QSet<QString> m_registeredServices;
    QVector<KRunnerMatch> m_pendingMatches;
    QString m_query;
    QTimer m_queryTimer;
    int m_pendingReplies = 0;
    quint64 m_generation = 0;
    bool m_discoveryFinished = false;
};

} // namespace kestrel

Q_DECLARE_METATYPE(kestrel::KRunnerRemoteMatch)
Q_DECLARE_METATYPE(kestrel::KRunnerRemoteMatches)
Q_DECLARE_METATYPE(QVector<kestrel::KRunnerMatch>)
