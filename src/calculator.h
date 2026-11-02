#pragma once

#include <QDateTime>
#include <QString>

namespace kestrel {

/// Result of one inline evaluation: arithmetic, unit conversion or date/time.
struct CalculatorResult
{
    enum class Kind { None, Expression, Conversion, DateTime };

    Kind kind = Kind::None;
    QString title;    ///< the answer, shown as the result title ("42", "6.2137 mi")
    QString subtitle; ///< the input, shown underneath ("6 * 7", "10 km")
    QString value;    ///< text copied when the result is activated

    bool isValid() const { return kind != Kind::None; }
};

/// Evaluates `query` as an arithmetic expression, a unit conversion (`10 km to mi`) or a
/// date/time phrase (`today`, `in 3 days`). Returns an invalid result when nothing matches.
/// Pure and allocation-light: safe to call on every keystroke.
CalculatorResult evaluateCalculator(const QString &query,
                                    const QDateTime &now = QDateTime::currentDateTime());

} // namespace kestrel
