#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

namespace kestrel {

/// Frecency anchors, learned search terms and aliases.
///
/// Persistence: a single `QSettings` INI file. Writes are not triggered inline;
/// they are coalesced into one write 2 seconds later. No disk I/O on the search path.
class UsageStore : public QObject
{
    Q_OBJECT

public:
    struct Entry
    {
        double anchor = 0.0;   ///< frecency anchor (Unix seconds)
        qint64 openedAt = 0;   ///< last launch (Unix seconds)
        QStringList terms;     ///< recent queries that opened this item
    };

    explicit UsageStore(QObject *parent = nullptr);
    ~UsageStore() override;

    Q_DISABLE_COPY_MOVE(UsageStore)

    Entry entry(const QString &id) const;
    double frecency(const QString &id, qint64 now) const;
    QString alias(const QString &id) const;

    void recordLaunch(const QString &id, const QString &query, qint64 now);
    void setAlias(const QString &id, const QString &alias);

    void flush();

private:
    void load();
    void scheduleFlush();

    QHash<QString, Entry> m_entries;
    QHash<QString, QString> m_aliases;
    bool m_dirty = false;
    qint64 m_lastFlush = 0;
};

} // namespace kestrel
