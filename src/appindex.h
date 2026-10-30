#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVector>

#include "desktopentry.h"

namespace kestrel {

/// In-memory index of installed applications, scanned off the main thread.
class AppIndex : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)

public:
    explicit AppIndex(QObject *parent = nullptr);

    int count() const { return static_cast<int>(m_entries.size()); }
    bool isLoading() const { return m_loading; }

    const QVector<DesktopEntry> &entries() const { return m_entries; }

    Q_INVOKABLE void reload();

signals:
    void countChanged();
    void loadingChanged();
    void ready(int total, int skipped);

private:
    void setLoading(bool loading);

    QVector<DesktopEntry> m_entries;
    bool m_loading = false;
};

} // namespace kestrel
