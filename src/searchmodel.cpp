#include "searchmodel.h"

#include "launcher.h"
#include "ranker.h"

#include <QDateTime>

#include <algorithm>

namespace kestrel {
namespace {

constexpr int kEmptyQueryLimit = 8;
constexpr int kResultLimit = 60;

// Score weights per field, mirroring Raycast's emphasis on title over metadata.
constexpr int kTitleWeight = 10;
constexpr int kKeywordWeight = 5;
constexpr int kGenericNameWeight = 3;
constexpr int kCommentWeight = 1;

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

} // namespace

SearchModel::SearchModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_usage(new UsageStore(this))
{
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
    emit queryChanged();
    rebuild();
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
    };
}

QVariant SearchModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_results.size()) {
        return {};
    }
    const Result &result = m_results.at(index.row());
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
    if (result.entryIndex < 0 || result.entryIndex >= m_entries.size()) {
        return {};
    }
    return m_entries.at(result.entryIndex).name;
}

bool SearchModel::launch(int row)
{
    if (row < 0 || row >= m_results.size()) {
        return false;
    }
    const Result &result = m_results.at(row);
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
    m_results.clear();

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const QString query = m_query.trimmed();
    const QString foldedQuery = foldText(query);

    if (query.isEmpty()) {
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
            return m_entries.at(a.entryIndex).name.localeAwareCompare(m_entries.at(b.entryIndex).name) < 0;
        });
        m_results = all.mid(0, kEmptyQueryLimit);
        endResetModel();
        emit countChanged();
        return;
    }

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

    m_results.reserve(std::min<int>(scored.size(), kResultLimit));
    for (int i = 0; i < scored.size() && i < kResultLimit; ++i) {
        Result result;
        result.entryIndex = scored.at(i).entryIndex;
        result.score = scored.at(i).score;
        result.frecency = scored.at(i).frecency;
        result.section = ResultsSection;
        m_results.append(result);
    }

    endResetModel();
    emit countChanged();
}

} // namespace kestrel
