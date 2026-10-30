#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QVector>

#include "appindex.h"
#include "krunnerclient.h"
#include "usagestore.h"

namespace kestrel {

/// Ranked, sectioned result list for one query.
///
/// Ranking ports Raycast's model (see docs/re/raycast-findings.md):
/// match classes first, then score, then frecency, then type priority, then collation.
class SearchModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(kestrel::AppIndex *index READ index WRITE setIndex NOTIFY indexChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        SubtitleRole,
        IconRole,
        SectionRole,
        KindRole,
        TerminalRole,
        SourceRole,
    };
    Q_ENUM(Roles)

    explicit SearchModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    AppIndex *index() const { return m_index; }
    void setIndex(AppIndex *indexed);

    QString query() const { return m_query; }
    void setQuery(const QString &query);

    int count() const { return static_cast<int>(m_results.size()); }

    /// Launches the row and records it for frecency + learned terms.
    Q_INVOKABLE bool launch(int row);

    /// Human-readable name of a row (used for copy actions).
    Q_INVOKABLE QString nameAt(int row) const;

signals:
    void indexChanged();
    void queryChanged();
    void countChanged();
    void launched(const QString &id);

private:
    struct Result
    {
        int entryIndex = -1;
        int runnerIndex = -1;
        bool isRunner = false;
        int score = 0;
        double frecency = 1.0;
        int section = 0;
    };

    void rebuild();
    void appendKRunnerMatches(const QString &query, const QVector<KRunnerMatch> &matches);

    AppIndex *m_index = nullptr;
    UsageStore *m_usage = nullptr;
    KRunnerClient *m_krunnerClient = nullptr;
    QVector<DesktopEntry> m_entries;
    QVector<KRunnerMatch> m_krunnerMatches;
    QString m_query;
    QVector<Result> m_results;
};

} // namespace kestrel
