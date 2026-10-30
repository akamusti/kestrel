#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

namespace kestrel {

class AutostartManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool setupPromptRequired READ setupPromptRequired
                   NOTIFY setupPromptRequiredChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    explicit AutostartManager(QObject *parent = nullptr);

    bool setupPromptRequired() const { return m_setupPromptRequired; }
    QString errorMessage() const { return m_errorMessage; }

    Q_INVOKABLE bool enableAtLogin();
    Q_INVOKABLE void dismissSetupPrompt();

signals:
    void setupPromptRequiredChanged();
    void errorMessageChanged();

private:
    void setSetupPromptRequired(bool required);
    void setErrorMessage(const QString &message);
    void rememberPromptWasHandled();

    bool m_setupPromptRequired = false;
    QString m_errorMessage;
};

} // namespace kestrel
