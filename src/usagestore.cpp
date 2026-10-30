#include "usagestore.h"

#include "ranker.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

namespace kestrel {
namespace {

constexpr qsizetype kMaxTerms = 8;
constexpr int kFlushDelayMs = 2000;

QString usageFilePath()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation);
    const QString dir = base.isEmpty() ? QDir::homePath() + QStringLiteral("/.local/state/kestrel")
                                       : base + QStringLiteral("/kestrel");
    return dir + QStringLiteral("/usage.ini");
}

} // namespace

UsageStore::UsageStore(QObject *parent)
    : QObject(parent)
{
    load();
}

UsageStore::~UsageStore()
{
    if (m_dirty) {
        flush();
    }
}

void UsageStore::load()
{
    QSettings settings(usageFilePath(), QSettings::IniFormat);
    for (const QString &id : settings.childGroups()) {
        settings.beginGroup(id);
        Entry entry;
        entry.anchor = settings.value(QStringLiteral("anchor"), 0.0).toDouble();
        entry.openedAt = settings.value(QStringLiteral("openedAt"), 0).toLongLong();
        entry.terms = settings.value(QStringLiteral("terms")).toStringList();
        const QString alias = settings.value(QStringLiteral("alias")).toString();
        settings.endGroup();

        if (!alias.isEmpty()) {
            m_aliases.insert(id, alias);
        }
        if (entry.anchor > 0.0) {
            m_entries.insert(id, entry);
        }
    }
}

UsageStore::Entry UsageStore::entry(const QString &id) const
{
    return m_entries.value(id);
}

double UsageStore::frecency(const QString &id, qint64 now) const
{
    return frecencyScore(m_entries.value(id).anchor, now);
}

QString UsageStore::alias(const QString &id) const
{
    return m_aliases.value(id);
}

void UsageStore::recordLaunch(const QString &id, const QString &query, qint64 now)
{
    if (id.isEmpty()) {
        return;
    }

    Entry &entry = m_entries[id];
    entry.anchor = updatedAnchor(entry.anchor, now);
    entry.openedAt = now;

    const QString trimmed = query.trimmed();
    if (!trimmed.isEmpty()) {
        entry.terms.removeAll(trimmed);
        entry.terms.prepend(trimmed);
        while (entry.terms.size() > kMaxTerms) {
            entry.terms.removeLast();
        }
    }

    m_dirty = true;
    scheduleFlush();
}

void UsageStore::setAlias(const QString &id, const QString &alias)
{
    const QString trimmed = alias.trimmed();
    if (trimmed.isEmpty()) {
        if (m_aliases.remove(id) > 0) {
            m_dirty = true;
            scheduleFlush();
        }
        return;
    }
    m_aliases.insert(id, trimmed);
    m_dirty = true;
    scheduleFlush();
}

void UsageStore::scheduleFlush()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    // If a write already happened recently, one deferred write is enough.
    if (now - m_lastFlush < kFlushDelayMs) {
        return;
    }
    m_lastFlush = now;
    QTimer::singleShot(kFlushDelayMs, this, &UsageStore::flush);
}

void UsageStore::flush()
{
    if (!m_dirty) {
        return;
    }

    const QString path = usageFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSettings settings(path, QSettings::IniFormat);
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        settings.beginGroup(it.key());
        settings.setValue(QStringLiteral("anchor"), it->anchor);
        settings.setValue(QStringLiteral("openedAt"), it->openedAt);
        settings.setValue(QStringLiteral("terms"), it->terms);
        const QString alias = m_aliases.value(it.key());
        if (!alias.isEmpty()) {
            settings.setValue(QStringLiteral("alias"), alias);
        }
        settings.endGroup();
    }
    for (auto it = m_aliases.constBegin(); it != m_aliases.constEnd(); ++it) {
        if (m_entries.contains(it.key())) {
            continue;
        }
        settings.beginGroup(it.key());
        settings.setValue(QStringLiteral("alias"), it.value());
        settings.endGroup();
    }
    settings.sync();

    m_dirty = false;
}

} // namespace kestrel
