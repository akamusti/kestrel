#include "searchmodel.h"

#include "launcher.h"
#include "ranker.h"

#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QSettings>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>

namespace kestrel {
namespace {

constexpr int kEmptyQueryLimit = 8;
constexpr int kResultLimit = 60;
constexpr int kWebFallbackDelayMs = 150;

// Score weights per field, mirroring Raycast's emphasis on title over metadata.
constexpr int kTitleWeight = 10;
constexpr int kKeywordWeight = 5;
constexpr int kGenericNameWeight = 3;
constexpr int kCommentWeight = 1;

constexpr auto kDefaultWebSearch = "https://duckduckgo.com/?q=%1";

enum Section { SuggestionsSection = 0, ResultsSection = 1 };

struct Scored
{
    int entryIndex = -1;
    int score = kNoMatch;
    double frecency = 1.0;
    int termTier = 0;
    int aliasTier = 0; // 0 = none, 1 = prefix, 2 = exact
    bool exactTitle = false;
    bool prefixTitle = false;
    int typePriority = 5;
};

QVariantMap makeAction(const QString &id, const QString &title, const QString &shortcut = {})
{
    QVariantMap action;
    action.insert(QStringLiteral("id"), id);
    action.insert(QStringLiteral("title"), title);
    action.insert(QStringLiteral("shortcut"), shortcut);
    return action;
}

} // namespace

SearchModel::SearchModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_usage(new UsageStore(this))
    , m_krunnerClient(new KRunnerClient(this))
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Kestrel"), QStringLiteral("kestrel"));
    m_webSearchTemplate =
        settings.value(QStringLiteral("web/searchUrl"), QString::fromLatin1(kDefaultWebSearch))
            .toString();
    if (!m_webSearchTemplate.contains(QStringLiteral("%1"))) {
        m_webSearchTemplate = QString::fromLatin1(kDefaultWebSearch);
    }

    connect(m_krunnerClient, &KRunnerClient::matchesReady, this,
            &SearchModel::appendKRunnerMatches);

    // Give local and KRunner sources a beat to answer before offering the web fallback,
    // so the row does not flash for queries that do have local results.
    m_webFallbackTimer.setSingleShot(true);
    m_webFallbackTimer.setInterval(kWebFallbackDelayMs);
    connect(&m_webFallbackTimer, &QTimer::timeout, this, &SearchModel::showWebFallback);
}

void SearchModel::setIndex(AppIndex *indexed)
{
    if (m_index == indexed) {
        return;
    }
    if (m_index) {
        disconnect(m_index, nullptr, this, nullptr);
    }
    m_index = indexed;
    if (m_index) {
        connect(m_index, &AppIndex::ready, this, [this] {
            m_entries = m_index->entries();
            rebuild();
        });
        m_entries = m_index->entries();
    } else {
        m_entries.clear();
    }
    rebuild();
    emit indexChanged();
}

void SearchModel::setQuery(const QString &query)
{
    if (m_query == query) {
        return;
    }
    m_query = query;
    m_krunnerMatches.clear();
    emit queryChanged();
    rebuild();
    m_krunnerClient->search(query);
}

void SearchModel::setFilter(Filter filter)
{
    if (m_filter == filter) {
        return;
    }
    m_filter = filter;
    rebuild();
    emit filterChanged();
}

QString SearchModel::filterLabel() const
{
    switch (m_filter) {
    case Filter::All:
        return tr("All");
    case Filter::Applications:
        return tr("Applications");
    case Filter::KRunner:
        return tr("KRunner");
    case Filter::Calculator:
        return tr("Calculator");
    }
    return {};
}

void SearchModel::nextFilter()
{
    switch (m_filter) {
    case Filter::All:
        setFilter(Filter::Applications);
        break;
    case Filter::Applications:
        setFilter(Filter::KRunner);
        break;
    case Filter::KRunner:
        setFilter(Filter::Calculator);
        break;
    case Filter::Calculator:
        setFilter(Filter::All);
        break;
    }
}

int SearchModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_results.size());
}

QHash<int, QByteArray> SearchModel::roleNames() const
{
    return {
        {IdRole, QByteArrayLiteral("appId")},
        {NameRole, QByteArrayLiteral("name")},
        {SubtitleRole, QByteArrayLiteral("subtitle")},
        {IconRole, QByteArrayLiteral("iconName")},
        {SectionRole, QByteArrayLiteral("section")},
        {KindRole, QByteArrayLiteral("kind")},
        {TerminalRole, QByteArrayLiteral("isTerminal")},
        {SourceRole, QByteArrayLiteral("source")},
    };
}

QVariant SearchModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_results.size()) {
        return {};
    }
    const Result &result = m_results.at(index.row());

    if (result.kind == ResultKind::Calculator) {
        switch (role) {
        case IdRole:
            return QStringLiteral("calculator/") + m_query.trimmed();
        case NameRole:
            return m_calculator.title;
        case SubtitleRole:
            return m_calculator.subtitle;
        case IconRole:
            return QStringLiteral("accessories-calculator");
        case SectionRole:
            return QStringLiteral("calculator");
        case KindRole:
            return QStringLiteral("Calculator");
        case TerminalRole:
            return false;
        case SourceRole:
            return QStringLiteral("calculator");
        default:
            return {};
        }
    }

    if (result.kind == ResultKind::Web) {
        const QString url = webSearchUrl(m_query);
        switch (role) {
        case IdRole:
            return url;
        case NameRole:
            return tr("Search the web for “%1”").arg(m_query.trimmed());
        case SubtitleRole:
            return url;
        case IconRole:
            return QStringLiteral("system-search");
        case SectionRole:
            return QStringLiteral("web");
        case KindRole:
            return QStringLiteral("Web");
        case TerminalRole:
            return false;
        case SourceRole:
            return QStringLiteral("web");
        default:
            return {};
        }
    }

    if (result.kind == ResultKind::Runner) {
        if (result.runnerIndex < 0 || result.runnerIndex >= m_krunnerMatches.size()) {
            return {};
        }
        const KRunnerMatch &match = m_krunnerMatches.at(result.runnerIndex);
        switch (role) {
        case IdRole:
            return match.service + QLatin1Char('/') + match.id;
        case NameRole:
            return match.text;
        case SubtitleRole:
            return match.subtitle.isEmpty() ? match.category : match.subtitle;
        case IconRole:
            return match.iconName.isEmpty() ? QStringLiteral("system-search") : match.iconName;
        case SectionRole:
            return QStringLiteral("krunner");
        case KindRole:
            return QStringLiteral("KRunner");
        case TerminalRole:
            return false;
        case SourceRole:
            return QStringLiteral("krunner");
        default:
            return {};
        }
    }

    if (result.entryIndex < 0 || result.entryIndex >= m_entries.size()) {
        return {};
    }
    const DesktopEntry &entry = m_entries.at(result.entryIndex);

    switch (role) {
    case IdRole:
        return entry.id;
    case NameRole:
        return entry.name;
    case SubtitleRole:
        return entry.comment.isEmpty() ? entry.genericName : entry.comment;
    case IconRole:
        return entry.iconName.isEmpty() ? QStringLiteral("application-x-executable") : entry.iconName;
    case SectionRole:
        return result.section == SuggestionsSection ? QStringLiteral("suggestions")
                                                    : QStringLiteral("results");
    case KindRole:
        return entry.terminal ? QStringLiteral("Terminal") : QStringLiteral("Application");
    case TerminalRole:
        return entry.terminal;
    case SourceRole:
        return QStringLiteral("application");
    default:
        return {};
    }
}

QString SearchModel::nameAt(int row) const
{
    if (row < 0 || row >= m_results.size()) {
        return {};
    }
    const Result &result = m_results.at(row);
    switch (result.kind) {
    case ResultKind::Calculator:
        return m_calculator.title;
    case ResultKind::Web:
        return tr("Search the web for “%1”").arg(m_query.trimmed());
    case ResultKind::Runner:
        if (result.runnerIndex < 0 || result.runnerIndex >= m_krunnerMatches.size()) {
            return {};
        }
        return m_krunnerMatches.at(result.runnerIndex).text;
    case ResultKind::Entry:
        break;
    }
    if (result.entryIndex < 0 || result.entryIndex >= m_entries.size()) {
        return {};
    }
    return m_entries.at(result.entryIndex).name;
}

QString SearchModel::sourceAt(int row) const
{
    if (row < 0 || row >= m_results.size()) {
        return {};
    }
    switch (m_results.at(row).kind) {
    case ResultKind::Calculator:
        return QStringLiteral("calculator");
    case ResultKind::Web:
        return QStringLiteral("web");
    case ResultKind::Runner:
        return QStringLiteral("krunner");
    case ResultKind::Entry:
        break;
    }
    return QStringLiteral("application");
}

QVariantList SearchModel::actionsAt(int row) const
{
    QVariantList actions;
    if (row < 0 || row >= m_results.size()) {
        return actions;
    }

    const Result &result = m_results.at(row);
    switch (result.kind) {
    case ResultKind::Calculator:
        actions.append(makeAction(QStringLiteral("copy-result"), tr("Copy Result"),
                                  QStringLiteral("↵")));
        break;
    case ResultKind::Web:
        actions.append(makeAction(QStringLiteral("open"), tr("Search the Web"),
                                  QStringLiteral("↵")));
        actions.append(makeAction(QStringLiteral("copy-url"), tr("Copy URL")));
        break;
    case ResultKind::Runner:
        actions.append(makeAction(QStringLiteral("open"), tr("Run"), QStringLiteral("↵")));
        actions.append(makeAction(QStringLiteral("copy-name"), tr("Copy Name"),
                                  QStringLiteral("Ctrl+Shift+C")));
        actions.append(makeAction(QStringLiteral("copy-id"), tr("Copy Identifier")));
        break;
    case ResultKind::Entry:
        actions.append(makeAction(QStringLiteral("open"), tr("Open"), QStringLiteral("↵")));
        actions.append(makeAction(QStringLiteral("copy-name"), tr("Copy Name"),
                                  QStringLiteral("Ctrl+Shift+C")));
        actions.append(makeAction(QStringLiteral("copy-id"), tr("Copy Desktop ID")));
        actions.append(makeAction(QStringLiteral("copy-exec"), tr("Copy Command")));
        actions.append(makeAction(QStringLiteral("open-desktop-file"),
                                  tr("Open Desktop File")));
        break;
    }
    return actions;
}

bool SearchModel::runAction(int row, const QString &actionId)
{
    if (row < 0 || row >= m_results.size()) {
        return false;
    }
    const Result &result = m_results.at(row);
    if (actionId == QLatin1String("open")) {
        return launch(row);
    }
    if (actionId == QLatin1String("copy-name")) {
        return copyToClipboard(nameAt(row));
    }

    switch (result.kind) {
    case ResultKind::Calculator:
        return actionId == QLatin1String("copy-result") && copyToClipboard(m_calculator.value);

    case ResultKind::Web:
        return actionId == QLatin1String("copy-url")
            && copyToClipboard(webSearchUrl(m_query));

    case ResultKind::Runner: {
        if (result.runnerIndex < 0 || result.runnerIndex >= m_krunnerMatches.size()) {
            return false;
        }
        const KRunnerMatch &match = m_krunnerMatches.at(result.runnerIndex);
        if (actionId == QLatin1String("copy-id")) {
            return copyToClipboard(match.service + QLatin1Char('/') + match.id);
        }
        return false;
    }

    case ResultKind::Entry:
        break;
    }

    if (result.entryIndex < 0 || result.entryIndex >= m_entries.size()) {
        return false;
    }
    const DesktopEntry &entry = m_entries.at(result.entryIndex);
    if (actionId == QLatin1String("copy-id")) {
        return copyToClipboard(entry.id);
    }
    if (actionId == QLatin1String("copy-exec")) {
        return copyToClipboard(launchCommand(entry).join(u' '));
    }
    if (actionId == QLatin1String("open-desktop-file")) {
        return QDesktopServices::openUrl(QUrl::fromLocalFile(entry.desktopFile));
    }
    return false;
}

bool SearchModel::launch(int row)
{
    if (row < 0 || row >= m_results.size()) {
        return false;
    }
    const Result &result = m_results.at(row);

    switch (result.kind) {
    case ResultKind::Calculator:
        return copyToClipboard(m_calculator.value);

    case ResultKind::Web:
        return QDesktopServices::openUrl(QUrl(webSearchUrl(m_query)));

    case ResultKind::Runner: {
        if (result.runnerIndex < 0 || result.runnerIndex >= m_krunnerMatches.size()) {
            return false;
        }
        m_krunnerClient->run(m_krunnerMatches.at(result.runnerIndex));
        return true;
    }

    case ResultKind::Entry:
        break;
    }

    if (result.entryIndex < 0 || result.entryIndex >= m_entries.size()) {
        return false;
    }

    const DesktopEntry &entry = m_entries.at(result.entryIndex);
    if (!kestrel::launch(entry)) {
        return false;
    }

    m_usage->recordLaunch(entry.id, m_query, QDateTime::currentSecsSinceEpoch());
    emit launched(entry.id);
    return true;
}

void SearchModel::rebuild()
{
    beginResetModel();
    m_webFallbackTimer.stop();
    m_results.clear();

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const QString query = m_query.trimmed();
    const QString foldedQuery = foldText(query);

    const bool showApplications = m_filter == Filter::All || m_filter == Filter::Applications;
    const bool showCalculator = m_filter == Filter::All || m_filter == Filter::Calculator;
    const bool showRunner = m_filter == Filter::All || m_filter == Filter::KRunner;

    if (query.isEmpty()) {
        if (showApplications) {
            QVector<Result> all;
            all.reserve(m_entries.size());
            for (int i = 0; i < m_entries.size(); ++i) {
                Result result;
                result.entryIndex = i;
                result.frecency = m_usage->frecency(m_entries.at(i).id, now);
                result.section = SuggestionsSection;
                all.append(result);
            }
            std::stable_sort(all.begin(), all.end(), [this](const Result &a, const Result &b) {
                if (a.frecency != b.frecency) {
                    return a.frecency > b.frecency;
                }
                return m_entries.at(a.entryIndex)
                           .name.localeAwareCompare(m_entries.at(b.entryIndex).name)
                    < 0;
            });
            m_results = all.mid(0, kEmptyQueryLimit);
        }
        endResetModel();
        emit countChanged();
        return;
    }

    m_calculator = evaluateCalculator(query);

    QVector<Scored> scored;
    scored.reserve(m_entries.size());

    const auto fieldScore = [&foldedQuery, &query](const QString &text, int weight) {
        if (text.isEmpty()) {
            return kNoMatch;
        }
        int skipped = 0;
        const int raw = fuzzyScore(foldText(text), foldedQuery, &skipped);
        if (raw == kNoMatch) {
            return kNoMatch;
        }
        if (!passesThreshold(raw, static_cast<int>(query.size()), skipped, Sensitivity::Medium)) {
            return kNoMatch;
        }
        return raw * weight;
    };

    if (showApplications) {
        for (int i = 0; i < m_entries.size(); ++i) {
            const DesktopEntry &entry = m_entries.at(i);
            const UsageStore::Entry usage = m_usage->entry(entry.id);

            Scored item;
            item.entryIndex = i;
            item.frecency = frecencyScore(usage.anchor, now);

            const QString foldedName = foldText(entry.name);
            item.exactTitle = foldedName == foldedQuery;
            item.prefixTitle = !item.exactTitle && foldedName.startsWith(foldedQuery);

            const QString alias = m_usage->alias(entry.id);
            if (!alias.isEmpty()) {
                const QString foldedAlias = foldText(alias);
                if (foldedAlias == foldedQuery) {
                    item.aliasTier = 2;
                } else if (foldedAlias.startsWith(foldedQuery)) {
                    item.aliasTier = 1;
                }
            }

            for (const QString &term : usage.terms) {
                item.termTier = std::max(item.termTier, searchTermTier(term, query));
            }

            item.score = std::max({fieldScore(entry.name, kTitleWeight),
                                   fieldScore(entry.genericName, kGenericNameWeight),
                                   fieldScore(entry.comment, kCommentWeight)});

            int keywordScore = kNoMatch;
            for (const QString &keyword : entry.keywords) {
                keywordScore = std::max(keywordScore, fieldScore(keyword, kKeywordWeight));
            }
            item.score = std::max(item.score, keywordScore);

            const bool matched = item.aliasTier > 0 || item.termTier > 0 || item.exactTitle
                || item.prefixTitle || item.score != kNoMatch;
            if (matched) {
                scored.append(item);
            }
        }
    }

    std::stable_sort(scored.begin(), scored.end(), [this](const Scored &a, const Scored &b) {
        // 1. exact alias
        const bool aExactAlias = a.aliasTier == 2;
        const bool bExactAlias = b.aliasTier == 2;
        if (aExactAlias != bExactAlias) {
            return aExactAlias;
        }

        // 2. exact learned search term
        const bool aExactTerm = a.termTier == 3;
        const bool bExactTerm = b.termTier == 3;
        if (aExactTerm != bExactTerm) {
            return aExactTerm;
        }
        if (aExactTerm && a.frecency != b.frecency) {
            return a.frecency > b.frecency;
        }

        // 3. exact title
        if (a.exactTitle != b.exactTitle) {
            return a.exactTitle;
        }
        if (a.exactTitle && a.frecency != b.frecency) {
            return a.frecency > b.frecency;
        }

        // 4. prefix: alias, then title, then learned term
        const bool aAliasPrefix = a.aliasTier == 1;
        const bool bAliasPrefix = b.aliasTier == 1;
        if (aAliasPrefix != bAliasPrefix) {
            return aAliasPrefix;
        }
        if (a.prefixTitle != b.prefixTitle) {
            return a.prefixTitle;
        }
        const bool aTermPrefix = a.termTier == 2;
        const bool bTermPrefix = b.termTier == 2;
        if (aTermPrefix != bTermPrefix) {
            return aTermPrefix;
        }

        // 5. overbounds (1-3 extra characters) beats no term match
        if (a.termTier != b.termTier) {
            return a.termTier > b.termTier;
        }

        // 6. match quality
        if (a.score != b.score) {
            return a.score > b.score;
        }

        // 7. frecency
        if (a.frecency != b.frecency) {
            return a.frecency > b.frecency;
        }

        // 8. type priority (applications all tie today; kept for later sources)
        if (a.typePriority != b.typePriority) {
            return a.typePriority > b.typePriority;
        }

        // 9. stable, locale-aware title order
        return m_entries.at(a.entryIndex).name.localeAwareCompare(m_entries.at(b.entryIndex).name) < 0;
    });

    if (showCalculator && m_calculator.isValid()) {
        Result result;
        result.kind = ResultKind::Calculator;
        result.section = ResultsSection;
        m_results.append(result);
    }

    if (showApplications) {
        m_results.reserve(m_results.size() + std::min<qsizetype>(scored.size(), kResultLimit)
                          + (showRunner ? m_krunnerMatches.size() : 0) + 1);
        for (int i = 0; i < scored.size() && i < kResultLimit; ++i) {
            Result result;
            result.entryIndex = scored.at(i).entryIndex;
            result.score = scored.at(i).score;
            result.frecency = scored.at(i).frecency;
            result.section = ResultsSection;
            m_results.append(result);
        }
    }

    if (showRunner) {
        appendRunnerRows();
    }

    // The web fallback is the last resort: no local result and the index has finished loading.
    if (m_filter == Filter::All && m_results.isEmpty()
        && !(m_index && m_index->isLoading())) {
        scheduleWebFallback();
    }

    endResetModel();
    emit countChanged();
}

void SearchModel::scheduleWebFallback()
{
    if (m_query.trimmed().isEmpty()) {
        return;
    }
    m_webFallbackTimer.start();
}

void SearchModel::showWebFallback()
{
    if (m_filter != Filter::All || m_query.trimmed().isEmpty() || !m_results.isEmpty()) {
        return;
    }
    if (m_index && m_index->isLoading()) {
        return;
    }

    const int row = static_cast<int>(m_results.size());
    beginInsertRows(QModelIndex(), row, row);
    Result result;
    result.kind = ResultKind::Web;
    result.section = ResultsSection;
    m_results.append(result);
    endInsertRows();
    emit countChanged();
}

void SearchModel::appendRunnerRows()
{
    m_results.reserve(m_results.size() + m_krunnerMatches.size());
    for (int i = 0; i < m_krunnerMatches.size(); ++i) {
        Result result;
        result.kind = ResultKind::Runner;
        result.runnerIndex = i;
        result.section = ResultsSection;
        m_results.append(result);
    }
}

void SearchModel::appendKRunnerMatches(const QString &query,
                                       const QVector<KRunnerMatch> &matches)
{
    if (query != m_query.trimmed() || query.isEmpty() || matches.isEmpty()) {
        return;
    }

    m_krunnerMatches = matches;
    m_webFallbackTimer.stop();
    if (m_filter != Filter::All && m_filter != Filter::KRunner) {
        return;
    }

    // KRunner just delivered local results, so the web fallback is no longer the last resort.
    if (!m_results.isEmpty() && m_results.constLast().kind == ResultKind::Web) {
        const int row = static_cast<int>(m_results.size()) - 1;
        beginRemoveRows(QModelIndex(), row, row);
        m_results.removeLast();
        endRemoveRows();
    }

    const int first = static_cast<int>(m_results.size());
    beginInsertRows(QModelIndex(), first, first + matches.size() - 1);
    appendRunnerRows();
    endInsertRows();
    emit countChanged();
}

QString SearchModel::webSearchUrl(const QString &query) const
{
    const QString encoded = QString::fromUtf8(QUrl::toPercentEncoding(query.trimmed()));
    return m_webSearchTemplate.arg(encoded);
}

bool SearchModel::copyToClipboard(const QString &text) const
{
    if (text.isEmpty()) {
        return false;
    }
    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text);
        return true;
    }
    return false;
}

} // namespace kestrel
