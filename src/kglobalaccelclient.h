#pragma once

#include <QDBusConnection>
#include <QDBusInterface>
#include <QObject>
#include <QStringList>

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
    void handleShortcutPressed(const QString &componentUnique, const QString &actionUnique,
                               qlonglong timestamp);

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
