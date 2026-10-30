#pragma once

#include <QDBusConnection>
#include <QDBusInterface>
#include <QObject>
#include <QStringList>
#include <QVariantMap>

class QDBusPendingCallWatcher;

namespace kestrel {

class KGlobalAccelClient : public QObject
{
    Q_OBJECT

public:
    explicit KGlobalAccelClient(QObject *parent = nullptr);
    ~KGlobalAccelClient() override;

    void registerToggleShortcut();

signals:
    void toggleRequested();

private slots:
    void handleShortcutPressed(const QString &actionId, const QVariantMap &options);

private:
    void registerComponent();
    void connectComponent(const QString &objectPath);
    void setActiveShortcut();
    void setDefaultShortcut();

    QDBusConnection m_bus;
    QDBusInterface m_accelInterface;
    QStringList m_actionId;
    bool m_registrationStarted = false;
    bool m_registered = false;
};

} // namespace kestrel
