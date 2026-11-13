#include "launcher.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>

namespace kestrel {

namespace {
/// Index of the actual program inside an expanded command that may be wrapped in
/// `env VAR=value ...`. Returns -1 when no program could be found.
int programIndex(const QStringList &command)
{
    int index = 0;
    if (index < command.size() && QFileInfo(command.at(index)).fileName() == QLatin1String("env")) {
        ++index;
    }
    while (index < command.size()) {
        const QString &token = command.at(index);
        if (token.startsWith(u'/') || !token.contains(u'=')) {
            break;
        }
        ++index;
    }
    return (index < command.size()) ? index : -1;
}

bool activateOverDbus(const DesktopEntry &entry)
{
    QString service = entry.id;
    if (!service.endsWith(QStringLiteral(".desktop"))) {
        return false;
    }
    service.chop(QStringLiteral(".desktop").size());
    if (service.isEmpty()) {
        return false;
    }

    QString objectPath = service;
    objectPath.replace(u'.', u'/');
    objectPath.replace(u'-', u'_');
    objectPath.prepend(u'/');

    QDBusInterface application(service, objectPath, QStringLiteral("org.freedesktop.Application"),
                               QDBusConnection::sessionBus());
    if (!application.isValid()) {
        return false;
    }

    // DBusActivatable applications own their process and desktop identity when
    // started through their standard activation interface.
    application.asyncCall(QStringLiteral("Activate"), QVariantMap{});
    return true;
}
} // namespace

bool launch(const DesktopEntry &entry)
{
    if (entry.dbusActivatable && activateOverDbus(entry)) {
        return true;
    }

    const QStringList command = launchCommand(entry);
    if (command.isEmpty()) {
        return false;
    }

    const int index = programIndex(command);
    if (index < 0) {
        return false;
    }

    const QString program = command.at(index);
    const QStringList arguments = command.mid(index + 1);

    // Carry over any `VAR=value` assignments that preceded the program.
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    int envIndex = 0;
    if (envIndex < command.size() && QFileInfo(command.at(envIndex)).fileName() == QLatin1String("env")) {
        ++envIndex;
    }
    while (envIndex < index) {
        const QString &token = command.at(envIndex);
        const qsizetype eq = token.indexOf(u'=');
        if (eq > 0) {
            environment.insert(token.left(eq), token.mid(eq + 1));
        }
        ++envIndex;
    }

    // AppImages are sometimes shipped without the executable bit; fix that lazily
    // so launching does not silently fail.
    const QFileInfo programInfo(program);
    if (programInfo.exists() && !programInfo.isExecutable()) {
        QFile::setPermissions(program,
                              programInfo.permissions() | QFileDevice::ExeUser
                                  | QFileDevice::ExeGroup | QFileDevice::ExeOther);
    }

    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setProcessEnvironment(environment);

    if (!entry.workingDir.isEmpty() && QDir(entry.workingDir).exists()) {
        process.setWorkingDirectory(entry.workingDir);
    }

    return process.startDetached();
}

bool terminate(const DesktopEntry &entry)
{
    QString binary = entry.startupWmClass;
    if (binary.isEmpty()) {
        const QStringList command = launchCommand(entry);
        if (command.isEmpty()) {
            return false;
        }
        const int index = programIndex(command);
        if (index < 0) {
            return false;
        }
        binary = QFileInfo(command.at(index)).fileName();
    }
    return QProcess::startDetached(QStringLiteral("pkill"), {QStringLiteral("-f"), binary});
}

} // namespace kestrel
