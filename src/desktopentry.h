#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace kestrel {

/// The subset of an XDG desktop entry a launcher needs.
struct DesktopEntry
{
    QString id;          ///< "org.kde.dolphin.desktop"
    QString desktopFile; ///< absolute path
    QString name;
    QString genericName;
    QString comment;
    QString iconName;
    QString execRaw;
    QString workingDir;
    QString startupWmClass;
    QStringList keywords;
    QStringList categories;
    bool terminal = false;
    bool dbusActivatable = false;
};

struct DesktopScan
{
    QVector<DesktopEntry> entries;
    int filesRead = 0;
    int filesSkipped = 0;
};

/// `applications` directories from the XDG data dirs, in precedence order.
QVector<QString> applicationDirectories();

/// Parses a single .desktop file. Returns an empty entry when it is hidden or not an application.
DesktopEntry parseDesktopFile(const QString &path, bool *ok = nullptr);

/// Scans every XDG directory; when an id appears twice, the first one wins.
DesktopScan scanDesktopEntries();

/// Splits the Exec line into arguments and expands field codes (%f, %U, %i, %c, %k, ...).
QStringList expandExec(const DesktopEntry &entry);

/// File name of the binary an Exec line runs, skipping `env` and `VAR=value` prefixes.
QString executableFileName(const QString &execRaw);

/// Builds the command line to run, wrapping terminal applications in a terminal emulator.
QStringList launchCommand(const DesktopEntry &entry);

} // namespace kestrel
