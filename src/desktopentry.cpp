#include "desktopentry.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>

#include <algorithm>

namespace kestrel {
namespace {

/// Resolves the escapes allowed in .desktop string values (\s \n \t \r \\).
QString unescapeValue(const QString &value)
{
    QString out;
    out.reserve(value.size());
    for (qsizetype i = 0; i < value.size(); ++i) {
        const QChar c = value.at(i);
        if (c != u'\\' || i + 1 >= value.size()) {
            out += c;
            continue;
        }
        const QChar next = value.at(++i);
        switch (next.unicode()) {
        case u's':
            out += u' ';
            break;
        case u'n':
            out += u'\n';
            break;
        case u't':
            out += u'\t';
            break;
        case u'r':
            out += u'\r';
            break;
        case u';':
            out += u';';
            break;
        case u'\\':
            out += u'\\';
            break;
        default:
            out += next;
            break;
        }
    }
    return out;
}

/// Splits `a;b;c;` style lists while preserving escaped `\;` characters.
QStringList splitList(const QString &value)
{
    QStringList items;
    QString current;
    for (qsizetype i = 0; i < value.size(); ++i) {
        const QChar c = value.at(i);
        if (c == u'\\' && i + 1 < value.size()) {
            current += c;
            current += value.at(++i);
            continue;
        }
        if (c == u';') {
            const QString item = unescapeValue(current).trimmed();
            if (!item.isEmpty()) {
                items.append(item);
            }
            current.clear();
            continue;
        }
        current += c;
    }
    const QString tail = unescapeValue(current).trimmed();
    if (!tail.isEmpty()) {
        items.append(tail);
    }
    return items;
}

bool parseInto(const QString &path, QHash<QString, QString> *values)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    bool inDesktopEntry = false;
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(u'#')) {
            continue;
        }
        if (line.startsWith(u'[')) {
            inDesktopEntry = line == QLatin1String("[Desktop Entry]");
            continue;
        }
        if (!inDesktopEntry) {
            continue;
        }
        const qsizetype eq = line.indexOf(u'=');
        if (eq <= 0) {
            continue;
        }
        // Per the specification: if a key appears more than once, the first one counts.
        const QString key = line.left(eq).trimmed();
        if (!values->contains(key)) {
            values->insert(key, line.mid(eq + 1).trimmed());
        }
    }
    return true;
}

QStringList localeTags()
{
    QString tag = QLocale::system().name(); // e.g. "tr_TR"
    if (const qsizetype at = tag.indexOf(u'@'); at > 0) {
        tag.truncate(at);
    }
    QStringList tags;
    if (tag.isEmpty() || tag == QLatin1String("C")) {
        return tags;
    }
    tag.replace(u'-', u'_');
    tags << tag;
    if (tag.size() > 2) {
        tags << tag.left(2);
    }
    return tags;
}

QString localizedValue(const QHash<QString, QString> &values, const QString &key)
{
    for (const QString &tag : localeTags()) {
        const QString localizedKey = key + u'[' + tag + u']';
        if (const auto it = values.constFind(localizedKey); it != values.constEnd()) {
            const QString value = unescapeValue(*it).trimmed();
            if (!value.isEmpty()) {
                return value;
            }
        }
    }
    return unescapeValue(values.value(key)).trimmed();
}

} // namespace

QString executableFileName(const QString &execRaw)
{
    qsizetype index = 0;
    const QStringList tokens = QProcess::splitCommand(execRaw);

    // `env VAR=value /path/app` and bare `VAR=value /path/app` both end up at the same binary.
    if (index < tokens.size() && QFileInfo(tokens.at(index)).fileName() == QLatin1String("env")) {
        ++index;
    }
    while (index < tokens.size()) {
        const QString &token = tokens.at(index);
        if (token.startsWith(u'/') || !token.contains(u'=')) {
            break;
        }
        ++index;
    }
    if (index >= tokens.size()) {
        return {};
    }
    return QFileInfo(tokens.at(index)).fileName();
}

QVector<QString> applicationDirectories()
{
    QStringList dirs;
    const QString dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (!dataHome.isEmpty()) {
        dirs << dataHome + QStringLiteral("/applications");
    }
    for (const QString &dir : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
        dirs << dir + QStringLiteral("/applications");
    }
    dirs.removeDuplicates();
    return QVector<QString>(dirs.cbegin(), dirs.cend());
}

DesktopEntry parseDesktopFile(const QString &path, bool *ok)
{
    if (ok) {
        *ok = false;
    }

    QHash<QString, QString> values;
    if (!parseInto(path, &values)) {
        return {};
    }
    if (values.value(QStringLiteral("Type")) != QLatin1String("Application")) {
        return {};
    }
    if (values.value(QStringLiteral("Hidden")) == QLatin1String("true")) {
        return {};
    }
    if (values.value(QStringLiteral("NoDisplay")) == QLatin1String("true")) {
        return {};
    }

    DesktopEntry entry;
    entry.desktopFile = path;
    entry.id = QFileInfo(path).fileName();
    entry.name = localizedValue(values, QStringLiteral("Name"));
    if (entry.name.isEmpty()) {
        entry.name = QFileInfo(path).completeBaseName();
    }
    entry.genericName = localizedValue(values, QStringLiteral("GenericName"));
    entry.comment = localizedValue(values, QStringLiteral("Comment"));
    entry.iconName = values.value(QStringLiteral("Icon")).trimmed();
    entry.execRaw = unescapeValue(values.value(QStringLiteral("Exec"))).trimmed();
    entry.workingDir = unescapeValue(values.value(QStringLiteral("Path"))).trimmed();
    entry.startupWmClass = values.value(QStringLiteral("StartupWMClass")).trimmed();
    entry.keywords = splitList(values.value(QStringLiteral("Keywords")));
    entry.categories = splitList(values.value(QStringLiteral("Categories")));
    entry.terminal = values.value(QStringLiteral("Terminal")) == QLatin1String("true");
    entry.dbusActivatable =
        values.value(QStringLiteral("DBusActivatable")) == QLatin1String("true");

    if (entry.execRaw.isEmpty()) {
        return {};
    }

    if (ok) {
        *ok = true;
    }
    return entry;
}

DesktopScan scanDesktopEntries()
{
    DesktopScan scan;
    QSet<QString> seen;
    QSet<QString> seenPrograms;

    for (const QString &directory : applicationDirectories()) {
        QDirIterator it(directory, {QStringLiteral("*.desktop")}, QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            ++scan.filesRead;

            bool ok = false;
            DesktopEntry entry = parseDesktopFile(path, &ok);
            if (!ok) {
                ++scan.filesSkipped;
                continue;
            }
            if (seen.contains(entry.id)) {
                ++scan.filesSkipped;
                continue;
            }
            seen.insert(entry.id);

            // Two entries for the same program (e.g. a hand-written and a generated
            // shortcut to one AppImage) would otherwise show up twice in the list.
            const QString binary = executableFileName(entry.execRaw);
            if (!binary.isEmpty()) {
                const QString programKey = entry.name.toCaseFolded() + u'\n' + binary;
                if (seenPrograms.contains(programKey)) {
                    ++scan.filesSkipped;
                    continue;
                }
                seenPrograms.insert(programKey);
            }

            scan.entries.push_back(std::move(entry));
        }
    }

    std::sort(scan.entries.begin(), scan.entries.end(), [](const DesktopEntry &a, const DesktopEntry &b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });
    return scan;
}

QStringList expandExec(const DesktopEntry &entry)
{
    static const QRegularExpression dropCodes(QStringLiteral("%[fFuUdDnNvmi]"));

    QStringList out;
    const QStringList tokens = QProcess::splitCommand(entry.execRaw);
    out.reserve(tokens.size());

    for (QString token : tokens) {
        token.remove(dropCodes);
        token.replace(QStringLiteral("%%"), QStringLiteral("%"));
        token.replace(QStringLiteral("%c"), entry.name);
        token.replace(QStringLiteral("%k"), entry.desktopFile);
        if (!token.isEmpty()) {
            out.append(token);
        }
    }
    return out;
}

QStringList launchCommand(const DesktopEntry &entry)
{
    QStringList command = expandExec(entry);
    if (entry.terminal && !command.isEmpty()) {
        const QString terminal = qEnvironmentVariable("KESTREL_TERMINAL", QStringLiteral("konsole"));
        command.prepend(QStringLiteral("-e"));
        command.prepend(terminal);
    }
    return command;
}

} // namespace kestrel
