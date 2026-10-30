#include "ranker.h"

#include <QVector>

#include <algorithm>
#include <cmath>

namespace kestrel {
namespace {

/// ln(2) / 10 -> 10-day half-life.
constexpr double kLambda = 0.06931471805599453;
constexpr double kOverflowGuard = 709.78;
constexpr double kSecondsPerDay = 86400.0;

QChar foldChar(QChar c)
{
    return c.toCaseFolded();
}

bool isWordStart(const QString &target, qsizetype index)
{
    if (index == 0) {
        return true;
    }
    const QChar previous = target.at(index - 1);
    if (isSeparator(previous)) {
        return true;
    }
    return previous.isLower() && target.at(index).isUpper();
}

} // namespace

QString foldText(const QString &text)
{
    return text.toCaseFolded();
}

bool isSeparator(QChar c)
{
    switch (c.unicode()) {
    case u'\t':
    case u'\n':
    case u' ':
    case u'-':
    case u'.':
    case u'/':
    case u'(':
    case u')':
    case u'[':
    case u']':
        return true;
    default:
        return false;
    }
}

int fuzzyScore(const QString &target, const QString &query, int *skipped)
{
    if (skipped) {
        *skipped = 0;
    }
    if (query.isEmpty()) {
        return 0;
    }
    if (target == query) {
        return 10000;
    }

    const qsizetype n = query.size();
    const qsizetype m = target.size();

    qsizetype separatorCount = 0;
    for (const QChar c : query) {
        if (isSeparator(c)) {
            ++separatorCount;
        }
    }
    const qsizetype u = n - separatorCount;
    if (u > m) {
        return kNoMatch;
    }

    // Cheap pre-check: every non-separator query character must appear in order.
    if (u > 2) {
        qsizetype t = 0;
        for (const QChar qc : query) {
            if (isSeparator(qc)) {
                continue;
            }
            while (t < m && foldChar(target.at(t)) != foldChar(qc)) {
                ++t;
            }
            if (t >= m) {
                return kNoMatch;
            }
            ++t;
        }
    }

    QVector<int> previous(m, kNoMatch);
    QVector<int> current(m, kNoMatch);

    qsizetype firstMatchColumn = -1; // first matched column of the previous row
    qsizetype matchedRows = 0;
    int skippedCount = 0;

    for (qsizetype row = 0; row < n; ++row) {
        const QChar qc = foldChar(query.at(row));
        const bool queryIsSeparator = isSeparator(qc);
        std::fill(current.begin(), current.end(), kNoMatch);

        const qsizetype upperBound = m - (u - 1 - matchedRows);
        qsizetype firstInRow = -1;

        for (qsizetype col = firstMatchColumn + 1; col < upperBound; ++col) {
            const QChar tc = foldChar(target.at(col));
            const bool separatorMatch = queryIsSeparator && isSeparator(tc);
            if (!separatorMatch && tc != qc) {
                continue;
            }

            int base = 2;
            if (separatorMatch) {
                base = 1;
            } else if (row == 0 && col == 0) {
                base = 4;
            } else if (isWordStart(target, col) && !isSeparator(tc)) {
                base = 3;
            }

            int cell = kNoMatch;
            if (row == 0) {
                cell = base;
            } else {
                for (qsizetype r = firstMatchColumn; r < col; ++r) {
                    if (previous.at(r) == kNoMatch) {
                        continue;
                    }
                    const int penalty = (r == col - 1) ? 0 : 1;
                    const int candidate = previous.at(r) + base - penalty;
                    cell = std::max(cell, candidate);
                }
            }

            if (cell == kNoMatch) {
                continue;
            }
            current[col] = cell;
            if (firstInRow < 0) {
                firstInRow = col;
            }
        }

        if (firstInRow >= 0) {
            firstMatchColumn = firstInRow;
            ++matchedRows;
            // The next row consumes this row's scores.
            previous = current;
            continue;
        }

        if (queryIsSeparator && row != n - 1) {
            ++skippedCount;
            // Separator with no counterpart: drop the row, keep the previous one.
            continue;
        }
        if (skipped) {
            *skipped = skippedCount;
        }
        return kNoMatch;
    }

    int best = kNoMatch;
    for (const int value : previous) {
        best = std::max(best, value);
    }

    if (skipped) {
        *skipped = skippedCount;
    }
    return best == kNoMatch ? kNoMatch : best;
}

bool passesThreshold(int score, int queryLength, int skipped, Sensitivity sensitivity)
{
    if (score == kNoMatch) {
        return false;
    }
    switch (sensitivity) {
    case Sensitivity::Low:
        return true;
    case Sensitivity::Medium:
        return score >= 1.5 * (queryLength - skipped - 2) + 4;
    case Sensitivity::High:
        return score > 2 * (queryLength - skipped);
    }
    return true;
}

int searchTermTier(const QString &term, const QString &query)
{
    if (term.isEmpty() || query.isEmpty()) {
        return 0;
    }
    const QString foldedTerm = term.toCaseFolded();
    const QString foldedQuery = query.toCaseFolded();
    if (foldedTerm == foldedQuery) {
        return 3;
    }
    if (foldedTerm.size() > foldedQuery.size() && foldedTerm.startsWith(foldedQuery)) {
        return 2;
    }
    if (foldedQuery.size() > foldedTerm.size() && foldedQuery.startsWith(foldedTerm)) {
        const qsizetype leftover = foldedQuery.size() - foldedTerm.size();
        if (leftover >= 1 && leftover <= 3) {
            return 1;
        }
    }
    return 0;
}

double frecencyScore(double anchorSeconds, qint64 nowSeconds)
{
    if (!(anchorSeconds > 0.0)) {
        return 1.0;
    }
    const double exponent = kLambda * (anchorSeconds - static_cast<double>(nowSeconds)) / kSecondsPerDay;
    return std::max(1.0, std::exp(std::min(kOverflowGuard, exponent)));
}

double updatedAnchor(double anchorSeconds, qint64 nowSeconds)
{
    const double now = static_cast<double>(nowSeconds);
    const double anchor = std::max(anchorSeconds > 0.0 ? anchorSeconds : now, now);
    const double exponent = kLambda * (anchor - now) / kSecondsPerDay;
    const double score = std::max(1.0, std::exp(std::min(kOverflowGuard, exponent)));
    const double scoreAfterVisit = std::min(score + 100.0, 1e308);
    return now + kSecondsPerDay * std::log(scoreAfterVisit) / kLambda;
}

} // namespace kestrel
