#include "appindex.h"

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

namespace kestrel {

AppIndex::AppIndex(QObject *parent)
    : QObject(parent)
{
    // The scan runs on a worker thread; starting it here keeps QML free of setup work.
    reload();
}

void AppIndex::reload()
{
    if (m_loading) {
        return;
    }
    setLoading(true);

    auto *watcher = new QFutureWatcher<DesktopScan>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
        const DesktopScan scan = watcher->result();
        watcher->deleteLater();

        m_entries = scan.entries;
        setLoading(false);
        emit countChanged();
        emit ready(m_entries.size(), scan.filesSkipped);
    });
    watcher->setFuture(QtConcurrent::run(&scanDesktopEntries));
}

void AppIndex::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    emit loadingChanged();
}

} // namespace kestrel
