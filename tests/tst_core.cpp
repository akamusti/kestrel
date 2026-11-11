#include <QDateTime>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "autostartmanager.h"
#include "calculator.h"
#include "desktopentry.h"
#include "kglobalaccelclient.h"
#include "krunnerclient.h"
#include "ranker.h"
#include "searchmodel.h"
#include "usagestore.h"

using namespace kestrel;

class TestKRunner : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.krunner1")

public:
    int runCount() const { return m_runCount; }

public slots:
    KRunnerRemoteMatches Match(const QString &query)
    {
        if (query != QStringLiteral("calc")) {
            return {};
        }

        KRunnerRemoteMatch match;
        match.id = QStringLiteral("calculator-result");
        match.text = QStringLiteral("Calculator result");
        match.iconName = QStringLiteral("accessories-calculator");
        match.categoryRelevance = 100;
        match.relevance = 0.9;
        match.properties.insert(QStringLiteral("subtext"), QStringLiteral("42"));
        return {match};
    }

    void Run(const QString &matchId, const QString &actionId)
    {
        if (matchId == QStringLiteral("calculator-result") && actionId.isEmpty()) {
            ++m_runCount;
        }
    }

private:
    int m_runCount = 0;
};

class TestCore : public QObject
{
    Q_OBJECT

private slots:
    void exactMatchScoresHighest();
    void subsequenceMatches();
    void nonMatchingQueryReturnsNoMatch();
    void wordStartBeatsMidWord();
    void thresholdRejectsWeakMatches();
    void frecencyDecaysOverTime();
    void frecencyRewardsLaunches();
    void searchTermTierClassifies();
    void calculatorEvaluatesExpressions();
    void calculatorConvertsUnits();
    void calculatorHandlesDates();
    void parsesDesktopEntry();
    void skipsHiddenAndNonApplicationEntries();
    void expandsExecFieldCodes();
    void resolvesExecutableFileName();
    void globalShortcutIgnoresUnrelatedActions();
    void globalShortcutRequestsPaletteToggle();
    void autostartCanBeEnabledWithUserConsent();
    void autostartPromptCanBeDismissed();
    void searchModelShowsWebFallback();
    void searchModelFiltersResultSources();
    void parsesKRunnerDbusRunnerMetadata();
    void ignoresDisabledAndNonDbusRunners();
    void krunnerResultsAreAddedAndRunOverDbus();
};

void TestCore::exactMatchScoresHighest()
{
    const int exact = fuzzyScore(QStringLiteral("firefox"), QStringLiteral("firefox"));
    const int partial = fuzzyScore(QStringLiteral("firefox"), QStringLiteral("fire"));
    QVERIFY(exact > partial);
    QVERIFY(partial > 0);
}

void TestCore::subsequenceMatches()
{
    const int score = fuzzyScore(QStringLiteral("visual studio code"), QStringLiteral("vsc"));
    QVERIFY(score != kNoMatch);
}

void TestCore::nonMatchingQueryReturnsNoMatch()
{
    QCOMPARE(fuzzyScore(QStringLiteral("dolphin"), QStringLiteral("zzz")), kNoMatch);
    QCOMPARE(fuzzyScore(QStringLiteral("ab"), QStringLiteral("abc")), kNoMatch);
}

void TestCore::wordStartBeatsMidWord()
{
    const int wordStart = fuzzyScore(QStringLiteral("kate editor"), QStringLiteral("edit"));
    const int midWord = fuzzyScore(QStringLiteral("mediterranean"), QStringLiteral("edit"));
    QVERIFY(wordStart > midWord);
}

void TestCore::thresholdRejectsWeakMatches()
{
    const QString target = QStringLiteral("kdenlive");
    const QString query = QStringLiteral("kdlv");
    int skipped = 0;
    const int score = fuzzyScore(target, query, &skipped);
    QVERIFY(score != kNoMatch);

    QVERIFY(!passesThreshold(kNoMatch, 4, 0, Sensitivity::Low));
    QVERIFY(passesThreshold(score, 4, skipped, Sensitivity::Low));
    QVERIFY(passesThreshold(fuzzyScore(target, target), 7, 0, Sensitivity::High));
}

void TestCore::frecencyDecaysOverTime()
{
    const qint64 now = 1'700'000'000;
    const double anchor = updatedAnchor(0.0, now);
    const double fresh = frecencyScore(anchor, now);
    const double tenDaysLater = frecencyScore(anchor, now + 10 * 86400LL);
    const double twentyDaysLater = frecencyScore(anchor, now + 20 * 86400LL);

    QVERIFY(fresh > tenDaysLater);
    // Half-life is 10 days: the score halves, but never drops below 1.
    QVERIFY(qAbs(tenDaysLater - fresh / 2.0) < 1.0);
    QVERIFY(twentyDaysLater >= 1.0);
}

void TestCore::frecencyRewardsLaunches()
{
    const qint64 now = 1'700'000'000;
    const double before = 1.0;
    const double anchor = updatedAnchor(0.0, now);
    const double after = frecencyScore(anchor, now);

    QVERIFY(after > before);
    QVERIFY(after > 100.0);

    const double second = frecencyScore(updatedAnchor(anchor, now), now);
    QVERIFY(second > after);
}

void TestCore::searchTermTierClassifies()
{
    QCOMPARE(searchTermTier(QStringLiteral("ff"), QStringLiteral("ff")), 3);
    QCOMPARE(searchTermTier(QStringLiteral("firefox"), QStringLiteral("fire")), 2);
    QCOMPARE(searchTermTier(QStringLiteral("fi"), QStringLiteral("fir")), 1);
    QCOMPARE(searchTermTier(QStringLiteral("ff"), QStringLiteral("chrom")), 0);
    QCOMPARE(searchTermTier(QString(), QStringLiteral("ff")), 0);
}

void TestCore::calculatorEvaluatesExpressions()
{
    const QDateTime now(QDate(2026, 11, 2), QTime(15, 30));

    CalculatorResult result = evaluateCalculator(QStringLiteral("2 + 3 * 4"), now);
    QVERIFY(result.isValid());
    QCOMPARE(result.kind, CalculatorResult::Kind::Expression);
    QCOMPARE(result.title, QStringLiteral("14"));
    QCOMPARE(result.value, QStringLiteral("14"));

    result = evaluateCalculator(QStringLiteral("(1 + 2) ^ 3"), now);
    QCOMPARE(result.title, QStringLiteral("27"));

    result = evaluateCalculator(QStringLiteral("20% of 50"), now);
    QCOMPARE(result.title, QStringLiteral("10"));

    result = evaluateCalculator(QStringLiteral("sqrt(81)"), now);
    QCOMPARE(result.title, QStringLiteral("9"));

    // A bare number is not an expression: typing "42" should not hijack the result list.
    QVERIFY(!evaluateCalculator(QStringLiteral("42"), now).isValid());
    QVERIFY(!evaluateCalculator(QStringLiteral("firefox"), now).isValid());
    QVERIFY(!evaluateCalculator(QString(), now).isValid());
}

void TestCore::calculatorConvertsUnits()
{
    const QDateTime now(QDate(2026, 11, 2), QTime(15, 30));

    CalculatorResult result = evaluateCalculator(QStringLiteral("10 km to mi"), now);
    QVERIFY(result.isValid());
    QCOMPARE(result.kind, CalculatorResult::Kind::Conversion);
    QCOMPARE(result.value, QStringLiteral("6.2137119224"));
    QCOMPARE(result.title, QStringLiteral("6.2137119224 mi"));

    result = evaluateCalculator(QStringLiteral("100 c to f"), now);
    QCOMPARE(result.value, QStringLiteral("212"));
    QCOMPARE(result.title, QStringLiteral("212 °F"));

    // Units may touch the number ("10km") and the keyword may be "in".
    result = evaluateCalculator(QStringLiteral("1kg in g"), now);
    QCOMPARE(result.value, QStringLiteral("1000"));

    QVERIFY(!evaluateCalculator(QStringLiteral("10 km to kg"), now).isValid());
    QVERIFY(!evaluateCalculator(QStringLiteral("10 zz to mi"), now).isValid());
}

void TestCore::calculatorHandlesDates()
{
    const QDateTime now(QDate(2026, 11, 2), QTime(15, 30));

    const CalculatorResult today = evaluateCalculator(QStringLiteral("today"), now);
    QVERIFY(today.isValid());
    QCOMPARE(today.kind, CalculatorResult::Kind::DateTime);
    QCOMPARE(today.value, QLocale().toString(QDate(2026, 11, 2), QLocale::LongFormat));

    const CalculatorResult tomorrow = evaluateCalculator(QStringLiteral("tomorrow"), now);
    QCOMPARE(tomorrow.value, QLocale().toString(QDate(2026, 11, 3), QLocale::LongFormat));

    const CalculatorResult friday = evaluateCalculator(QStringLiteral("friday"), now);
    QCOMPARE(friday.value, QLocale().toString(QDate(2026, 11, 6), QLocale::LongFormat));

    const CalculatorResult later = evaluateCalculator(QStringLiteral("in 3 hours"), now);
    QCOMPARE(later.value,
             QLocale().toString(now.addSecs(3 * 3600), QLocale::ShortFormat));

    const CalculatorResult earlier = evaluateCalculator(QStringLiteral("2 days ago"), now);
    QCOMPARE(earlier.value,
             QLocale().toString(now.addDays(-2), QLocale::ShortFormat));
}

void TestCore::searchModelShowsWebFallback()
{
    SearchModel model;
    model.setQuery(QStringLiteral("zzzz no local match"));

    QTRY_COMPARE_WITH_TIMEOUT(model.count(), 1, 2000);
    const QAbstractItemModel *modelBase = &model;
    const QModelIndex row = modelBase->index(0, 0);
    QCOMPARE(model.data(row, SearchModel::SourceRole).toString(), QStringLiteral("web"));
    QVERIFY(model.data(row, SearchModel::NameRole).toString().contains(
        QStringLiteral("zzzz no local match")));
    QVERIFY(model.data(row, SearchModel::SubtitleRole).toString().startsWith(
        QStringLiteral("https://")));

    // Filters other than "All" never show the web fallback.
    model.setFilter(SearchModel::Filter::Applications);
    QCOMPARE(model.count(), 0);
    model.setFilter(SearchModel::Filter::All);
    QTRY_COMPARE_WITH_TIMEOUT(model.count(), 1, 2000);
}

void TestCore::searchModelFiltersResultSources()
{
    SearchModel model;
    model.setQuery(QStringLiteral("2 + 2"));

    QCOMPARE(model.count(), 1);
    const QAbstractItemModel *modelBase = &model;
    const QModelIndex row = modelBase->index(0, 0);
    QCOMPARE(model.data(row, SearchModel::SourceRole).toString(), QStringLiteral("calculator"));
    QCOMPARE(model.data(row, SearchModel::NameRole).toString(), QStringLiteral("4"));
    QCOMPARE(model.data(row, SearchModel::KindRole).toString(), QStringLiteral("Calculator"));

    const QVariantList actions = model.actionsAt(0);
    QCOMPARE(actions.size(), 1);
    QCOMPARE(actions.constFirst().toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("copy-result"));

    model.setFilter(SearchModel::Filter::Applications);
    QCOMPARE(model.count(), 0);

    model.setFilter(SearchModel::Filter::Calculator);
    QCOMPARE(model.count(), 1);

    model.nextFilter();
    QCOMPARE(model.filter(), SearchModel::Filter::All);
}

void TestCore::parsesDesktopEntry()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = dir.filePath(QStringLiteral("org.example.App.desktop"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("[Desktop Entry]\n"
               "Type=Application\n"
               "Name=Example App\n"
               "GenericName=Example\n"
               "Comment=Does example things\n"
               "Icon=example\n"
               "Exec=example --flag %U\n"
               "Terminal=true\n"
               "Keywords=ex;demo;\n"
               "Categories=Utility;\n");
    file.close();

    bool ok = false;
    const DesktopEntry entry = parseDesktopFile(path, &ok);
    QVERIFY(ok);
    QCOMPARE(entry.name, QStringLiteral("Example App"));
    QCOMPARE(entry.iconName, QStringLiteral("example"));
    QCOMPARE(entry.id, QStringLiteral("org.example.App.desktop"));
    QVERIFY(entry.terminal);
    QCOMPARE(entry.keywords, QStringList({QStringLiteral("ex"), QStringLiteral("demo")}));
}

void TestCore::skipsHiddenAndNonApplicationEntries()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString hidden = dir.filePath(QStringLiteral("hidden.desktop"));
    QFile hiddenFile(hidden);
    QVERIFY(hiddenFile.open(QIODevice::WriteOnly | QIODevice::Text));
    hiddenFile.write("[Desktop Entry]\nType=Application\nName=Hidden\nExec=hidden\nNoDisplay=true\n");
    hiddenFile.close();

    const QString link = dir.filePath(QStringLiteral("link.desktop"));
    QFile linkFile(link);
    QVERIFY(linkFile.open(QIODevice::WriteOnly | QIODevice::Text));
    linkFile.write("[Desktop Entry]\nType=Link\nName=Link\nURL=https://example.com\n");
    linkFile.close();

    bool ok = false;
    parseDesktopFile(hidden, &ok);
    QVERIFY(!ok);
    parseDesktopFile(link, &ok);
    QVERIFY(!ok);
}

void TestCore::expandsExecFieldCodes()
{
    DesktopEntry entry;
    entry.name = QStringLiteral("Example");
    entry.desktopFile = QStringLiteral("/usr/share/applications/example.desktop");
    entry.execRaw = QStringLiteral("example --open %f %i");
    entry.terminal = false;

    const QStringList command = expandExec(entry);
    QCOMPARE(command, QStringList({QStringLiteral("example"), QStringLiteral("--open")}));

    entry.terminal = true;
    const QStringList wrapped = launchCommand(entry);
    QVERIFY(wrapped.size() >= 3);
    QCOMPARE(wrapped.at(0), QStringLiteral("konsole"));
    QCOMPARE(wrapped.at(1), QStringLiteral("-e"));
    QCOMPARE(wrapped.at(2), QStringLiteral("example"));
}

void TestCore::resolvesExecutableFileName()
{
    QCOMPARE(executableFileName(QStringLiteral("/usr/bin/kate")), QStringLiteral("kate"));
    QCOMPARE(executableFileName(QStringLiteral("env DESKTOPINTEGRATION=1 /opt/app.AppImage")),
             QStringLiteral("app.AppImage"));
    QCOMPARE(executableFileName(QStringLiteral("SERVER=1 /opt/app %F")),
             QStringLiteral("app"));
    QCOMPARE(executableFileName(QString()), QString());
}

void TestCore::globalShortcutIgnoresUnrelatedActions()
{
    KGlobalAccelClient client;
    QSignalSpy toggleSpy(&client, &KGlobalAccelClient::toggleRequested);
    constexpr qlonglong timestamp = 0;

    QVERIFY(QMetaObject::invokeMethod(
        &client, "handleShortcutPressed", Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("Kestrel")),
        Q_ARG(QString, QStringLiteral("anotherAction")), Q_ARG(qlonglong, timestamp)));
    QVERIFY(QMetaObject::invokeMethod(
        &client, "handleShortcutPressed", Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("anotherComponent")),
        Q_ARG(QString, QStringLiteral("togglePalette")), Q_ARG(qlonglong, timestamp)));
    QCOMPARE(toggleSpy.count(), 0);
}

void TestCore::globalShortcutRequestsPaletteToggle()
{
    KGlobalAccelClient client;
    QSignalSpy toggleSpy(&client, &KGlobalAccelClient::toggleRequested);
    constexpr qlonglong timestamp = 0;

    QVERIFY(QMetaObject::invokeMethod(
        &client, "handleShortcutPressed", Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("Kestrel")),
        Q_ARG(QString, QStringLiteral("togglePalette")), Q_ARG(qlonglong, timestamp)));
    QCOMPARE(toggleSpy.count(), 1);
}

void TestCore::autostartCanBeEnabledWithUserConsent()
{
    QTemporaryDir configDir;
    QVERIFY(configDir.isValid());
    const QByteArray previousConfig = qgetenv("XDG_CONFIG_HOME");
    qputenv("XDG_CONFIG_HOME", configDir.path().toLocal8Bit());

    {
        AutostartManager manager;
        QVERIFY(manager.setupPromptRequired());
        QVERIFY(manager.enableAtLogin());
        QVERIFY(!manager.setupPromptRequired());

        const QString entryPath =
            configDir.filePath(QStringLiteral("autostart/dev.kestrel.Kestrel.desktop"));
        QFile entry(entryPath);
        QVERIFY(entry.open(QIODevice::ReadOnly | QIODevice::Text));
        const QByteArray contents = entry.readAll();
        QVERIFY(contents.contains("Exec=\""));
        QVERIFY(contents.contains("--start-hidden"));

        AutostartManager secondManager;
        QVERIFY(!secondManager.setupPromptRequired());
    }

    if (previousConfig.isNull()) {
        qunsetenv("XDG_CONFIG_HOME");
    } else {
        qputenv("XDG_CONFIG_HOME", previousConfig);
    }
}

void TestCore::autostartPromptCanBeDismissed()
{
    QTemporaryDir configDir;
    QVERIFY(configDir.isValid());
    const QByteArray previousConfig = qgetenv("XDG_CONFIG_HOME");
    qputenv("XDG_CONFIG_HOME", configDir.path().toLocal8Bit());

    {
        AutostartManager manager;
        QVERIFY(manager.setupPromptRequired());
        manager.dismissSetupPrompt();
        QVERIFY(!manager.setupPromptRequired());

        AutostartManager secondManager;
        QVERIFY(!secondManager.setupPromptRequired());
        QVERIFY(!QFileInfo::exists(
            configDir.filePath(QStringLiteral("autostart/dev.kestrel.Kestrel.desktop"))));
    }

    if (previousConfig.isNull()) {
        qunsetenv("XDG_CONFIG_HOME");
    } else {
        qputenv("XDG_CONFIG_HOME", previousConfig);
    }
}

void TestCore::parsesKRunnerDbusRunnerMetadata()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("runner.desktop"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("[Desktop Entry]\n"
               "X-KDE-ServiceTypes=Plasma/Runner\n"
               "X-Plasma-API=DBus\n"
               "X-Plasma-DBusRunner-Service=org.example.runner\n"
               "X-Plasma-DBusRunner-Path=/runner\n");
    file.close();

    const KRunnerDescriptor descriptor = parseKRunnerDescriptor(path);
    QCOMPARE(descriptor.service, QStringLiteral("org.example.runner"));
    QCOMPARE(descriptor.objectPath, QStringLiteral("/runner"));
}

void TestCore::ignoresDisabledAndNonDbusRunners()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("runner.desktop"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("[Desktop Entry]\n"
               "X-Plasma-API=DBus\n"
               "X-Plasma-DBusRunner-Service=org.example.runner\n"
               "X-KDE-PluginInfo-EnabledByDefault=false\n");
    file.close();
    QVERIFY(parseKRunnerDescriptor(path).service.isEmpty());

    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
    file.write("[Desktop Entry]\n"
               "X-Plasma-API=Command\n"
               "X-Plasma-DBusRunner-Service=org.example.runner\n");
    file.close();
    QVERIFY(parseKRunnerDescriptor(path).service.isEmpty());
}

void TestCore::krunnerResultsAreAddedAndRunOverDbus()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QSKIP("Session D-Bus is unavailable");
    }

    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());
    const QByteArray previousData = qgetenv("XDG_DATA_HOME");
    qputenv("XDG_DATA_HOME", dataDir.path().toLocal8Bit());

    QTemporaryDir homeDir;
    QVERIFY(homeDir.isValid());
    const QByteArray previousHome = qgetenv("HOME");
    qputenv("HOME", homeDir.path().toLocal8Bit());

    const QString serviceName =
        QStringLiteral("org.example.KestrelTestRunner%1").arg(QCoreApplication::applicationPid());
    const QString runnerDirectory = dataDir.filePath(QStringLiteral("kservices6"));
    QVERIFY(QDir().mkpath(runnerDirectory));
    QFile descriptor(QDir(runnerDirectory).filePath(QStringLiteral("test-runner.desktop")));
    QVERIFY(descriptor.open(QIODevice::WriteOnly | QIODevice::Text));
    descriptor.write(QStringLiteral("[Desktop Entry]\n"
                                    "X-Plasma-API=DBus\n"
                                    "X-Plasma-DBusRunner-Service=%1\n"
                                    "X-Plasma-DBusRunner-Path=/runner\n")
                         .arg(serviceName)
                         .toUtf8());
    descriptor.close();

    qDBusRegisterMetaType<KRunnerRemoteMatch>();
    qDBusRegisterMetaType<KRunnerRemoteMatches>();
    QVERIFY(bus.registerService(serviceName));
    TestKRunner runner;
    QVERIFY(bus.registerObject(QStringLiteral("/runner"), &runner,
                               QDBusConnection::ExportAllSlots));

    {
        SearchModel model;
        model.setQuery(QStringLiteral("calc"));
        const QAbstractItemModel *modelBase = &model;
        QTRY_COMPARE_WITH_TIMEOUT(
            model.data(modelBase->index(0, 0), SearchModel::SourceRole).toString(),
            QStringLiteral("krunner"), 5000);
        QCOMPARE(model.data(modelBase->index(0, 0), SearchModel::NameRole).toString(),
                 QStringLiteral("Calculator result"));
        QCOMPARE(model.data(modelBase->index(0, 0), SearchModel::SubtitleRole).toString(),
                 QStringLiteral("42"));
        QVERIFY(model.launch(0));
        QTRY_COMPARE_WITH_TIMEOUT(runner.runCount(), 1, 5000);
    }

    bus.unregisterObject(QStringLiteral("/runner"));
    bus.unregisterService(serviceName);
    if (previousData.isNull()) {
        qunsetenv("XDG_DATA_HOME");
    } else {
        qputenv("XDG_DATA_HOME", previousData);
    }
    if (previousHome.isNull()) {
        qunsetenv("HOME");
    } else {
        qputenv("HOME", previousHome);
    }
}

QTEST_GUILESS_MAIN(TestCore)
#include "tst_core.moc"
