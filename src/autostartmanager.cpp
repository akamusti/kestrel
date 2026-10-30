#include "autostartmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

namespace kestrel {
namespace {

constexpr auto kPromptHandledKey = "autostartPromptHandled";
constexpr auto kDesktopFileName = "dev.kestrel.Kestrel.desktop";

QSettings userSettings()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope,
                     QStringLiteral("Kestrel"), QStringLiteral("Kestrel"));
}

QString autostartFilePath()
{
    const QString configDirectory =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (configDirectory.isEmpty()) {
        return {};
    }
    return QDir(configDirectory).filePath(
        QStringLiteral("autostart/") + QString::fromLatin1(kDesktopFileName));
}

QString executablePath()
{
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    const QString path = appImage.isEmpty() ? QCoreApplication::applicationFilePath() : appImage;
    return QFileInfo(path).absoluteFilePath();
}

QString desktopExecArgument(QString path)
{
    path.replace(QLatin1Char('%'), QStringLiteral("%%"));
    path.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    path.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    path.replace(QLatin1Char('$'), QStringLiteral("\\$"));
    path.replace(QLatin1Char('`'), QStringLiteral("\\`"));
    return QLatin1Char('"') + path + QLatin1Char('"');
}

} // namespace

AutostartManager::AutostartManager(QObject *parent)
    : QObject(parent)
{
    const bool entryExists = !autostartFilePath().isEmpty()
                             && QFileInfo::exists(autostartFilePath());
    const bool promptHandled = userSettings().value(QLatin1String(kPromptHandledKey), false)
                                   .toBool();
    m_setupPromptRequired = !entryExists && !promptHandled;
}

bool AutostartManager::enableAtLogin()
{
    setErrorMessage(QString());
    const QString path = autostartFilePath();
    if (path.isEmpty()) {
        setErrorMessage(tr("Could not find your user configuration directory."));
        return false;
    }

    if (!QFileInfo::exists(path)) {
        const QString executable = executablePath();
        if (!QFileInfo(executable).isExecutable()) {
            setErrorMessage(tr("Could not locate the Kestrel executable."));
            return false;
        }

        const QFileInfo fileInfo(path);
        if (!QDir().mkpath(fileInfo.absolutePath())) {
            setErrorMessage(tr("Could not create the user autostart directory."));
            return false;
        }

        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            setErrorMessage(tr("Could not write the user autostart entry: %1")
                                .arg(file.errorString()));
            return false;
        }

        const QByteArray contents =
            QStringLiteral("[Desktop Entry]\n"
                           "Type=Application\n"
                           "Name=Kestrel\n"
                           "Comment=Keep the Kestrel shortcut available after sign-in\n"
                           "Exec=%1 --start-hidden\n"
                           "Terminal=false\n"
                           "X-KDE-autostart-enabled=true\n"
                           "X-GNOME-Autostart-enabled=true\n")
                .arg(desktopExecArgument(executable))
                .toUtf8();
        if (file.write(contents) != contents.size() || !file.commit()) {
            setErrorMessage(tr("Could not save the user autostart entry: %1")
                                .arg(file.errorString()));
            return false;
        }
    }

    rememberPromptWasHandled();
    setSetupPromptRequired(false);
    return true;
}

void AutostartManager::dismissSetupPrompt()
{
    rememberPromptWasHandled();
    setSetupPromptRequired(false);
}

void AutostartManager::setSetupPromptRequired(bool required)
{
    if (m_setupPromptRequired == required) {
        return;
    }
    m_setupPromptRequired = required;
    emit setupPromptRequiredChanged();
}

void AutostartManager::setErrorMessage(const QString &message)
{
    if (m_errorMessage == message) {
        return;
    }
    m_errorMessage = message;
    emit errorMessageChanged();
}

void AutostartManager::rememberPromptWasHandled()
{
    QSettings settings = userSettings();
    settings.setValue(QLatin1String(kPromptHandledKey), true);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qWarning("Could not save the autostart setup preference");
    }
}

} // namespace kestrel
