#pragma once

#include <QString>
#include <QStringList>

namespace kestrel {

/// Mirrors Raycast's `rootSearchSensitivity` setting.
enum class Sensitivity { Low, Medium, High };

inline constexpr int kNoMatch = -1;

/// Lowercases text for matching (no transliteration yet).
QString foldText(const QString &text);

bool isSeparator(QChar c);

/// Port of Raycast's `af()` matcher.
///
/// Returns `kNoMatch` when the query does not match. `skipped` receives the number of
/// separators in the query that had no counterpart in the target (used by the threshold).
int fuzzyScore(const QString &target, const QString &query, int *skipped = nullptr);

/// Threshold check: is `score` acceptable for this query length?
bool passesThreshold(int score, int queryLength, int skipped, Sensitivity sensitivity);

/// Learned search-term class: 0 = none, 1 = overbounds, 2 = prefix, 3 = exact.
int searchTermTier(const QString &term, const QString &query);

/// Frecency: exponential decay with a 10-day half-life.
/// `anchorSeconds` and `nowSeconds` are Unix timestamps.
double frecencyScore(double anchorSeconds, qint64 nowSeconds);

/// Computes the new `anchor` after a launch (+100 points).
double updatedAnchor(double anchorSeconds, qint64 nowSeconds);

} // namespace kestrel
