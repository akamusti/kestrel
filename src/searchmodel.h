#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantList>
#include <QVector>

#include "appindex.h"
#include "calculator.h"
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
    Q_PROPERTY(Filter filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(QString filterLabel READ filterLabel NOTIFY filterChanged)
    Q_PROPERTY(bool filterActive READ filterActive NOTIFY filterChanged)

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

    /// Result-source filter cycled with Ctrl+P.
    enum class Filter { All, Applications, KRunner, Calculator };
    Q_ENUM(Filter)

    explicit SearchModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    AppIndex *index() const { return m_index; }
    void setIndex(AppIndex *indexed);

    QString query() const { return m_query; }
    void setQuery(const QString &query);

    int count() const { return static_cast<int>(m_results.size()); }

    Filter filter() const { return m_filter; }
    void setFilter(Filter filter);
    QString filterLabel() const;
    bool filterActive() const { return m_filter != Filter::All; }
    Q_INVOKABLE void nextFilter();

    /// Launches the row and records applications for frecency + learned terms.
    Q_INVOKABLE bool launch(int row);

    /// Human-readable name of a row (used for copy actions).
    Q_INVOKABLE QString nameAt(int row) const;

    /// Stable source key of a row: `application`, `krunner`, `calculator` or `web`.
    Q_INVOKABLE QString sourceAt(int row) const;

    /// Actions available for a row, in display order. Each entry is
    /// `{ id, title, shortcut }` and is executed with runAction().
    Q_INVOKABLE QVariantList actionsAt(int row) const;
    Q_INVOKABLE bool runAction(int row, const QString &actionId);

signals:
    void indexChanged();
    void queryChanged();
    void countChanged();
    void filterChanged();
    void launched(const QString &id);

private:
    enum class ResultKind { Entry, Runner, Calculator, Web };

    struct Result
    {
        ResultKind kind = ResultKind::Entry;
        int entryIndex = -1;
        int runnerIndex = -1;
        int score = 0;
        double frecency = 1.0;
        int section = 0;
    };

    void rebuild();
    void appendRunnerRows();
    void scheduleWebFallback();
    void showWebFallback();
    void appendKRunnerMatches(const QString &query, const QVector<KRunnerMatch> &matches);
    QString webSearchUrl(const QString &query) const;
    bool copyToClipboard(const QString &text) const;

    AppIndex *m_index = nullptr;
    UsageStore *m_usage = nullptr;
    KRunnerClient *m_krunnerClient = nullptr;
    QVector<DesktopEntry> m_entries;
    QVector<KRunnerMatch> m_krunnerMatches;
    QString m_query;
    QString m_webSearchTemplate;
    CalculatorResult m_calculator;
    QVector<Result> m_results;
    QTimer m_webFallbackTimer;
    Filter m_filter = Filter::All;
};

} // namespace kestrel
