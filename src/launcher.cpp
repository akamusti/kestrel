#include "launcher.h"

#include <QDir>
#include <QProcess>

namespace kestrel {

bool launch(const DesktopEntry &entry)
{
    const QStringList command = launchCommand(entry);
    if (command.isEmpty()) {
        return false;
    }

    const QString program = command.constFirst();
    QStringList arguments = command.mid(1);

    if (!entry.workingDir.isEmpty() && QDir(entry.workingDir).exists()) {
        return QProcess::startDetached(program, arguments, entry.workingDir);
    }
    return QProcess::startDetached(program, arguments);
}

bool terminate(const DesktopEntry &entry)
{
    QString binary = entry.startupWmClass;
    if (binary.isEmpty()) {
        const QStringList command = launchCommand(entry);
        if (command.isEmpty()) {
            return false;
        }
        binary = QFileInfo(command.constFirst()).fileName();
    }
    return QProcess::startDetached(QStringLiteral("pkill"), {QStringLiteral("-f"), binary});
}

} // namespace kestrel
