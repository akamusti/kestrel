#include "calculator.h"

#include <QHash>
#include <QLocale>
#include <QRegularExpression>
#include <QStringList>
#include <QVector>

#include <cmath>

namespace kestrel {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEuler = 2.71828182845904523536;

/// Trims a double to a human string: at most ten decimals, no trailing zeros, no "-0".
QString formatNumber(double value)
{
    if (!std::isfinite(value)) {
        return {};
    }
    if (value == 0.0) {
        return QStringLiteral("0");
    }

    const double magnitude = std::abs(value);
    QString text;
    if (magnitude >= 1e15 || magnitude < 1e-9) {
        text = QString::number(value, 'g', 10);
    } else {
        text = QString::number(value, 'f', 10);
        while (text.endsWith(u'0')) {
            text.chop(1);
        }
        if (text.endsWith(u'.')) {
            text.chop(1);
        }
    }
    return text == QLatin1String("-0") ? QStringLiteral("0") : text;
}

// --- Expression parser -------------------------------------------------------

enum class TokenType {
    Number,
    Name,
    Plus,
    Minus,
    Star,
    Slash,
    Caret,
    Percent,
    LeftParen,
    RightParen,
    End,
};

struct Token
{
    TokenType type = TokenType::End;
    double number = 0.0;
    QString name;
};

QVector<Token> tokenize(const QString &text, bool *ok)
{
    QVector<Token> tokens;
    qsizetype i = 0;
    const qsizetype size = text.size();

    while (i < size) {
        const QChar c = text.at(i);
        if (c.isSpace()) {
            ++i;
            continue;
        }

        if (c.isDigit() || ((c == u'.' || c == u',') && i + 1 < size && text.at(i + 1).isDigit())) {
            QString literal;
            bool seenSeparator = false;
            while (i < size) {
                const QChar digit = text.at(i);
                if (digit.isDigit()) {
                    literal += digit;
                    ++i;
                    continue;
                }
                if ((digit == u'.' || digit == u',') && !seenSeparator) {
                    seenSeparator = true;
                    literal += u'.';
                    ++i;
                    continue;
                }
                break;
            }
            bool numberOk = false;
            const double number = literal.toDouble(&numberOk);
            if (!numberOk) {
                *ok = false;
                return {};
            }
            tokens.append({TokenType::Number, number, {}});
            continue;
        }

        if (c.isLetter()) {
            QString name;
            while (i < size && text.at(i).isLetter()) {
                name += text.at(i);
                ++i;
            }
            tokens.append({TokenType::Name, 0.0, name});
            continue;
        }

        TokenType type = TokenType::End;
        switch (c.unicode()) {
        case u'+':
            type = TokenType::Plus;
            break;
        case u'-':
            type = TokenType::Minus;
            break;
        case u'*':
            type = TokenType::Star;
            break;
        case u'/':
            type = TokenType::Slash;
            break;
        case u'^':
            type = TokenType::Caret;
            break;
        case u'%':
            type = TokenType::Percent;
            break;
        case u'(':
            type = TokenType::LeftParen;
            break;
        case u')':
            type = TokenType::RightParen;
            break;
        default:
            *ok = false;
            return {};
        }
        tokens.append({type, 0.0, {}});
        ++i;
    }

    tokens.append({TokenType::End, 0.0, {}});
    return tokens;
}

class Parser
{
public:
    explicit Parser(const QVector<Token> &tokens)
        : m_tokens(tokens)
    {
    }

    bool evaluate(double *out)
    {
        const double value = parseExpression();
        if (!m_ok || m_position != m_tokens.size() - 1 || !m_meaningful
            || !std::isfinite(value)) {
            return false;
        }
        *out = value;
        return true;
    }

private:
    const Token &peek() const { return m_tokens.at(m_position); }

    const Token &advance()
    {
        const Token &token = m_tokens.at(m_position);
        ++m_position;
        return token;
    }

    bool accept(TokenType type)
    {
        if (peek().type != type) {
            return false;
        }
        ++m_position;
        return true;
    }

    bool isKeyword(const QString &keyword) const
    {
        return peek().type == TokenType::Name
            && peek().name.compare(keyword, Qt::CaseInsensitive) == 0;
    }

    double parseExpression()
    {
        double value = parseTerm();
        while (m_ok) {
            if (accept(TokenType::Plus)) {
                m_meaningful = true;
                value += parseTerm();
            } else if (accept(TokenType::Minus)) {
                m_meaningful = true;
                value -= parseTerm();
            } else {
                break;
            }
        }
        return value;
    }

    double parseTerm()
    {
        double value = parseUnary();
        while (m_ok) {
            if (accept(TokenType::Star)) {
                m_meaningful = true;
                value *= parseUnary();
            } else if (accept(TokenType::Slash)) {
                m_meaningful = true;
                value /= parseUnary();
            } else if (isKeyword(QStringLiteral("mod"))) {
                advance();
                m_meaningful = true;
                const double divisor = parseUnary();
                value = divisor == 0.0 ? std::nan("") : std::fmod(value, divisor);
            } else if (isKeyword(QStringLiteral("of"))) {
                advance();
                m_meaningful = true;
                value *= parseUnary();
            } else {
                break;
            }
        }
        return value;
    }

    double parseUnary()
    {
        if (accept(TokenType::Minus)) {
            return -parseUnary();
        }
        if (accept(TokenType::Plus)) {
            return parseUnary();
        }
        return parsePower();
    }

    double parsePower()
    {
        double value = parsePrimary();
        if (accept(TokenType::Caret)) {
            m_meaningful = true;
            value = std::pow(value, parseUnary());
        }
        return value;
    }

    double parsePrimary()
    {
        double value = parseAtom();
        while (m_ok && accept(TokenType::Percent)) {
            m_meaningful = true;
            value /= 100.0;
        }
        return value;
    }

    double parseAtom()
    {
        if (!m_ok) {
            return 0.0;
        }

        if (accept(TokenType::LeftParen)) {
            m_meaningful = true;
            const double value = parseExpression();
            if (!accept(TokenType::RightParen)) {
                m_ok = false;
            }
            return value;
        }

        if (peek().type == TokenType::Number) {
            return advance().number;
        }

        if (peek().type != TokenType::Name) {
            m_ok = false;
            return 0.0;
        }

        const QString name = advance().name.toCaseFolded();
        if (name == QLatin1String("pi")) {
            return kPi;
        }
        if (name == QLatin1String("tau")) {
            return 2.0 * kPi;
        }
        if (name == QLatin1String("e")) {
            return kEuler;
        }
        if (!accept(TokenType::LeftParen)) {
            m_ok = false;
            return 0.0;
        }

        m_meaningful = true;
        const double argument = parseExpression();
        if (!accept(TokenType::RightParen)) {
            m_ok = false;
            return 0.0;
        }
        return applyFunction(name, argument);
    }

    double applyFunction(const QString &name, double argument)
    {
        if (name == QLatin1String("sqrt")) {
            return std::sqrt(argument);
        }
        if (name == QLatin1String("cbrt")) {
            return std::cbrt(argument);
        }
        if (name == QLatin1String("abs")) {
            return std::abs(argument);
        }
        if (name == QLatin1String("floor")) {
            return std::floor(argument);
        }
        if (name == QLatin1String("ceil")) {
            return std::ceil(argument);
        }
        if (name == QLatin1String("round")) {
            return std::round(argument);
        }
        if (name == QLatin1String("sin")) {
            return std::sin(argument);
        }
        if (name == QLatin1String("cos")) {
            return std::cos(argument);
        }
        if (name == QLatin1String("tan")) {
            return std::tan(argument);
        }
        if (name == QLatin1String("asin")) {
            return std::asin(argument);
        }
        if (name == QLatin1String("acos")) {
            return std::acos(argument);
        }
        if (name == QLatin1String("atan")) {
            return std::atan(argument);
        }
        if (name == QLatin1String("sinh")) {
            return std::sinh(argument);
        }
        if (name == QLatin1String("cosh")) {
            return std::cosh(argument);
        }
        if (name == QLatin1String("tanh")) {
            return std::tanh(argument);
        }
        if (name == QLatin1String("ln")) {
            return std::log(argument);
        }
        if (name == QLatin1String("log")) {
            return std::log10(argument);
        }
        if (name == QLatin1String("exp")) {
            return std::exp(argument);
        }
        m_ok = false;
        return 0.0;
    }

    const QVector<Token> &m_tokens;
    qsizetype m_position = 0;
    bool m_ok = true;
    bool m_meaningful = false;
};

CalculatorResult evaluateExpression(const QString &query)
{
    bool tokensOk = true;
    const QVector<Token> tokens = tokenize(query, &tokensOk);
    if (!tokensOk) {
        return {};
    }

    double value = 0.0;
    Parser parser(tokens);
    if (!parser.evaluate(&value)) {
        return {};
    }

    const QString formatted = formatNumber(value);
    if (formatted.isEmpty()) {
        return {};
    }

    CalculatorResult result;
    result.kind = CalculatorResult::Kind::Expression;
    result.title = formatted;
    result.subtitle = query.trimmed();
    result.value = formatted;
    return result;
}

// --- Unit conversion --------------------------------------------------------

enum UnitCategory { Length = 0, Mass = 1, Data = 2, Time = 3, Volume = 4, Speed = 5 };

struct UnitInfo
{
    int category = Length;
    double factor = 1.0; ///< multiplier to the category's base unit
    QString symbol;
};

const QHash<QString, UnitInfo> &unitTable()
{
    static const QHash<QString, UnitInfo> table = [] {
        QHash<QString, UnitInfo> units;

        const auto add = [&units](int category, double factor, const QString &symbol,
                                  const QStringList &aliases) {
            const UnitInfo info{category, factor, symbol};
            units.insert(symbol.toCaseFolded(), info);
            for (const QString &alias : aliases) {
                units.insert(alias.toCaseFolded(), info);
            }
        };

        add(Length, 0.001, QStringLiteral("mm"),
            {QStringLiteral("millimeter"), QStringLiteral("millimeters"),
             QStringLiteral("millimetre"), QStringLiteral("millimetres")});
        add(Length, 0.01, QStringLiteral("cm"),
            {QStringLiteral("centimeter"), QStringLiteral("centimeters"),
             QStringLiteral("centimetre"), QStringLiteral("centimetres")});
        add(Length, 1.0, QStringLiteral("m"),
            {QStringLiteral("meter"), QStringLiteral("meters"), QStringLiteral("metre"),
             QStringLiteral("metres")});
        add(Length, 1000.0, QStringLiteral("km"),
            {QStringLiteral("kilometer"), QStringLiteral("kilometers"),
             QStringLiteral("kilometre"), QStringLiteral("kilometres")});
        add(Length, 0.0254, QStringLiteral("in"),
            {QStringLiteral("inch"), QStringLiteral("inches")});
        add(Length, 0.3048, QStringLiteral("ft"),
            {QStringLiteral("foot"), QStringLiteral("feet")});
        add(Length, 0.9144, QStringLiteral("yd"),
            {QStringLiteral("yard"), QStringLiteral("yards")});
        add(Length, 1609.344, QStringLiteral("mi"),
            {QStringLiteral("mile"), QStringLiteral("miles")});
        add(Length, 1852.0, QStringLiteral("nmi"),
            {QStringLiteral("nauticalmile"), QStringLiteral("nauticalmiles")});

        add(Mass, 0.000001, QStringLiteral("mg"),
            {QStringLiteral("milligram"), QStringLiteral("milligrams")});
        add(Mass, 0.001, QStringLiteral("g"),
            {QStringLiteral("gram"), QStringLiteral("grams")});
        add(Mass, 1.0, QStringLiteral("kg"),
            {QStringLiteral("kilogram"), QStringLiteral("kilograms")});
        add(Mass, 1000.0, QStringLiteral("t"),
            {QStringLiteral("tonne"), QStringLiteral("tonnes")});
        add(Mass, 0.028349523125, QStringLiteral("oz"),
            {QStringLiteral("ounce"), QStringLiteral("ounces")});
        add(Mass, 0.45359237, QStringLiteral("lb"),
            {QStringLiteral("pound"), QStringLiteral("pounds")});
        add(Mass, 6.35029318, QStringLiteral("st"),
            {QStringLiteral("stone"), QStringLiteral("stones")});

        add(Data, 1.0, QStringLiteral("B"),
            {QStringLiteral("byte"), QStringLiteral("bytes")});
        add(Data, 1000.0, QStringLiteral("KB"),
            {QStringLiteral("kilobyte"), QStringLiteral("kilobytes")});
        add(Data, 1'000'000.0, QStringLiteral("MB"),
            {QStringLiteral("megabyte"), QStringLiteral("megabytes")});
        add(Data, 1'000'000'000.0, QStringLiteral("GB"),
            {QStringLiteral("gigabyte"), QStringLiteral("gigabytes")});
        add(Data, 1'000'000'000'000.0, QStringLiteral("TB"),
            {QStringLiteral("terabyte"), QStringLiteral("terabytes")});
        add(Data, 1024.0, QStringLiteral("KiB"),
            {QStringLiteral("kibibyte"), QStringLiteral("kibibytes")});
        add(Data, 1024.0 * 1024.0, QStringLiteral("MiB"),
            {QStringLiteral("mebibyte"), QStringLiteral("mebibytes")});
        add(Data, 1024.0 * 1024.0 * 1024.0, QStringLiteral("GiB"),
            {QStringLiteral("gibibyte"), QStringLiteral("gibibytes")});
        add(Data, 1024.0 * 1024.0 * 1024.0 * 1024.0, QStringLiteral("TiB"),
            {QStringLiteral("tebibyte"), QStringLiteral("tebibytes")});

        add(Time, 0.001, QStringLiteral("ms"),
            {QStringLiteral("millisecond"), QStringLiteral("milliseconds")});
        add(Time, 1.0, QStringLiteral("s"),
            {QStringLiteral("sec"), QStringLiteral("secs"), QStringLiteral("second"),
             QStringLiteral("seconds")});
        add(Time, 60.0, QStringLiteral("min"),
            {QStringLiteral("minute"), QStringLiteral("minutes")});
        add(Time, 3600.0, QStringLiteral("h"),
            {QStringLiteral("hr"), QStringLiteral("hrs"), QStringLiteral("hour"),
             QStringLiteral("hours")});
        add(Time, 86'400.0, QStringLiteral("day"),
            {QStringLiteral("days")});
        add(Time, 604'800.0, QStringLiteral("week"),
            {QStringLiteral("weeks")});

        add(Volume, 0.001, QStringLiteral("ml"),
            {QStringLiteral("milliliter"), QStringLiteral("milliliters")});
        add(Volume, 1.0, QStringLiteral("l"),
            {QStringLiteral("liter"), QStringLiteral("liters"), QStringLiteral("litre"),
             QStringLiteral("litres")});
        add(Volume, 3.785411784, QStringLiteral("gal"),
            {QStringLiteral("gallon"), QStringLiteral("gallons")});
        add(Volume, 0.946352946, QStringLiteral("qt"),
            {QStringLiteral("quart"), QStringLiteral("quarts")});
        add(Volume, 0.473176473, QStringLiteral("pt"),
            {QStringLiteral("pint"), QStringLiteral("pints")});
        add(Volume, 0.2365882365, QStringLiteral("cup"),
            {QStringLiteral("cups")});
        add(Volume, 0.0295735295625, QStringLiteral("floz"),
            {QStringLiteral("fluidounce"), QStringLiteral("fluidounces")});

        add(Speed, 1.0, QStringLiteral("m/s"),
            {QStringLiteral("mps"), QStringLiteral("meterspersecond")});
        add(Speed, 0.277777778, QStringLiteral("km/h"),
            {QStringLiteral("kmh"), QStringLiteral("kph"), QStringLiteral("kilometersperhour")});
        add(Speed, 0.44704, QStringLiteral("mph"),
            {QStringLiteral("milesperhour")});
        add(Speed, 0.514444444, QStringLiteral("kn"),
            {QStringLiteral("knot"), QStringLiteral("knots")});

        return units;
    }();
    return table;
}

QString normalizeUnit(QString unit)
{
    unit = unit.trimmed().toCaseFolded();
    unit.remove(u'°');
    while (unit.endsWith(u'.')) {
        unit.chop(1);
    }
    return unit;
}

bool convertTemperature(double value, const QString &from, const QString &to, double *out)
{
    static const QStringList names{QStringLiteral("c"), QStringLiteral("celsius"),
                                   QStringLiteral("centigrade"), QStringLiteral("f"),
                                   QStringLiteral("fahrenheit"), QStringLiteral("k"),
                                   QStringLiteral("kelvin")};
    if (!names.contains(from) || !names.contains(to)) {
        return false;
    }

    const auto toCelsius = [](double input, const QString &unit) {
        if (unit.startsWith(u'c')) {
            return input;
        }
        if (unit.startsWith(u'f')) {
            return (input - 32.0) * 5.0 / 9.0;
        }
        return input - 273.15;
    };
    const auto fromCelsius = [](double input, const QString &unit) {
        if (unit.startsWith(u'c')) {
            return input;
        }
        if (unit.startsWith(u'f')) {
            return input * 9.0 / 5.0 + 32.0;
        }
        return input + 273.15;
    };

    *out = fromCelsius(toCelsius(value, from), to);
    return true;
}

CalculatorResult evaluateConversion(const QString &query)
{
    static const QRegularExpression pattern(
        QStringLiteral("^\\s*(-?\\d+(?:[.,]\\d+)?)\\s*([a-zA-Zµ°]+)\\s*"
                       "(?:to|in|->|→)\\s*([a-zA-Zµ°]+)\\s*$"),
        QRegularExpression::CaseInsensitiveOption);

    const QRegularExpressionMatch match = pattern.match(query);
    if (!match.hasMatch()) {
        return {};
    }

    QString numberText = match.captured(1);
    numberText.replace(u',', u'.');
    bool numberOk = false;
    const double value = numberText.toDouble(&numberOk);
    if (!numberOk) {
        return {};
    }

    const QString from = normalizeUnit(match.captured(2));
    const QString to = normalizeUnit(match.captured(3));

    double converted = 0.0;
    QString toSymbol;

    if (convertTemperature(value, from, to, &converted)) {
        toSymbol = to.startsWith(u'c') ? QStringLiteral("°C")
                  : to.startsWith(u'f') ? QStringLiteral("°F")
                                        : QStringLiteral("K");
    } else {
        const QHash<QString, UnitInfo> &units = unitTable();
        const auto fromIt = units.constFind(from);
        const auto toIt = units.constFind(to);
        if (fromIt == units.constEnd() || toIt == units.constEnd()
            || fromIt->category != toIt->category) {
            return {};
        }
        converted = value * fromIt->factor / toIt->factor;
        toSymbol = toIt->symbol;
    }

    const QString formatted = formatNumber(converted);
    if (formatted.isEmpty()) {
        return {};
    }

    CalculatorResult result;
    result.kind = CalculatorResult::Kind::Conversion;
    result.title = formatted + u' ' + toSymbol;
    result.subtitle = query.trimmed();
    result.value = formatted;
    return result;
}

// --- Date and time ----------------------------------------------------------

CalculatorResult evaluateDateTime(const QString &query, const QDateTime &now)
{
    const QString phrase = query.trimmed().toCaseFolded();
    if (phrase.isEmpty()) {
        return {};
    }

    const auto dateResult = [](const QDate &date, const QString &label) {
        const QString text = QLocale().toString(date, QLocale::LongFormat);
        CalculatorResult result;
        result.kind = CalculatorResult::Kind::DateTime;
        result.title = text;
        result.subtitle = label;
        result.value = text;
        return result;
    };

    const auto timeResult = [](const QDateTime &dateTime, const QString &label) {
        const QString text = QLocale().toString(dateTime, QLocale::ShortFormat);
        CalculatorResult result;
        result.kind = CalculatorResult::Kind::DateTime;
        result.title = text;
        result.subtitle = label;
        result.value = text;
        return result;
    };

    if (phrase == QLatin1String("now")) {
        return timeResult(now, QStringLiteral("Now"));
    }
    if (phrase == QLatin1String("today")) {
        return dateResult(now.date(), QStringLiteral("Today"));
    }
    if (phrase == QLatin1String("tomorrow")) {
        return dateResult(now.date().addDays(1), QStringLiteral("Tomorrow"));
    }
    if (phrase == QLatin1String("yesterday")) {
        return dateResult(now.date().addDays(-1), QStringLiteral("Yesterday"));
    }

    static const QStringList weekdays{
        QStringLiteral("monday"), QStringLiteral("tuesday"), QStringLiteral("wednesday"),
        QStringLiteral("thursday"), QStringLiteral("friday"), QStringLiteral("saturday"),
        QStringLiteral("sunday")};
    const qsizetype weekdayIndex = weekdays.indexOf(phrase);
    if (weekdayIndex >= 0) {
        const int today = now.date().dayOfWeek();
        const int target = static_cast<int>(weekdayIndex) + 1;
        const int delta = (target - today + 7) % 7;
        return dateResult(now.date().addDays(delta), QStringLiteral("Date"));
    }

    static const QRegularExpression relative(
        QStringLiteral("^(?:in\\s+(\\d+)\\s+(minute|hour|day|week)s?"
                       "|(\\d+)\\s+(minute|hour|day|week)s?\\s+ago)$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = relative.match(phrase);
    if (!match.hasMatch()) {
        return {};
    }

    const bool past = !match.captured(3).isEmpty();
    const int amount = (past ? match.captured(3) : match.captured(1)).toInt();
    const QString unit = past ? match.captured(4) : match.captured(2);
    const int sign = past ? -1 : 1;

    QDateTime target = now;
    if (unit.startsWith(u"minute")) {
        target = now.addSecs(sign * 60LL * amount);
    } else if (unit.startsWith(u"hour")) {
        target = now.addSecs(sign * 3600LL * amount);
    } else if (unit.startsWith(u"day")) {
        target = now.addDays(sign * amount);
    } else {
        target = now.addDays(sign * 7LL * amount);
    }

    const QString label = past ? QStringLiteral("%1 %2 ago").arg(amount).arg(
                                    amount == 1 ? unit : unit + u's')
                               : QStringLiteral("In %1 %2").arg(amount).arg(
                                     amount == 1 ? unit : unit + u's');
    return timeResult(target, label);
}

} // namespace

CalculatorResult evaluateCalculator(const QString &query, const QDateTime &now)
{
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const CalculatorResult dateTime = evaluateDateTime(trimmed, now);
    if (dateTime.isValid()) {
        return dateTime;
    }

    const CalculatorResult conversion = evaluateConversion(trimmed);
    if (conversion.isValid()) {
        return conversion;
    }

    return evaluateExpression(trimmed);
}

} // namespace kestrel
