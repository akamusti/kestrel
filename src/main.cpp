#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlContext>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

#include "iconimageprovider.h"
#include "kglobalaccelclient.h"

namespace {

/// Session-local socket name for the single-instance handshake.
constexpr auto kServerName = "kestrel-single-instance";

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Kestrel"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Kestrel"));
    QGuiApplication::setOrganizationName(QStringLiteral("Kestrel"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("kestrel.dev"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.2.2"));
    QGuiApplication::setDesktopFileName(QStringLiteral("dev.kestrel.Kestrel"));
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("system-search")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Keyboard-first command palette for KDE Plasma"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("toggle"), QStringLiteral("Toggle the running instance.")});
    parser.addOption({QStringLiteral("start-hidden"),
                      QStringLiteral("Start in the background without showing the palette.")});
    parser.addOption({QStringLiteral("quit-after-first-frame"),
                      QStringLiteral("Render one frame and exit (headless smoke test).")});
    parser.addOption({QStringLiteral("grab"),
                      QStringLiteral("Save a PNG of the first frame to <path>, then exit."),
                      QStringLiteral("path")});
    parser.process(app);

    // Single instance: hand the request to the running process and exit.
    QLocalSocket probe;
    probe.connectToServer(QString::fromLatin1(kServerName));
    if (probe.waitForConnected(150)) {
        if (parser.isSet(QStringLiteral("start-hidden"))) {
            return 0;
        }
        probe.write("toggle\n");
        probe.flush();
        probe.waitForBytesWritten(150);
        return 0;
    }

    QLocalServer::removeServer(QString::fromLatin1(kServerName));
    QLocalServer server;
    server.listen(QString::fromLatin1(kServerName));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(
        QStringLiteral("startHidden"), parser.isSet(QStringLiteral("start-hidden")));
    // QML warnings must never be silent: a missing module only shows up as an empty window,
    // so they are written straight to stderr instead of going through the Qt logging rules.
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, [](const QList<QQmlError> &warnings) {
        for (const QQmlError &error : warnings) {
            fprintf(stderr, "QML: %s\n", qPrintable(error.toString()));
        }
    });
    engine.addImageProvider(QStringLiteral("icon"), new kestrel::IconImageProvider);
    engine.loadFromModule("Kestrel", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    QObject *root = engine.rootObjects().constFirst();
    QObject::connect(&server, &QLocalServer::newConnection, root, [&server, root] {
        while (QLocalSocket *socket = server.nextPendingConnection()) {
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket, root] {
                if (socket->readAll().trimmed() == QByteArrayLiteral("toggle")) {
                    QMetaObject::invokeMethod(root, "toggle");
                }
            });
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
        }
    });

    auto *globalAccel = new kestrel::KGlobalAccelClient(&app);
    QObject::connect(globalAccel, &kestrel::KGlobalAccelClient::toggleRequested, root,
                     [root] { QMetaObject::invokeMethod(root, "toggle"); });
    globalAccel->registerToggleShortcut();

    const QString grabPath = parser.value(QStringLiteral("grab"));
    if (!grabPath.isEmpty()) {
        if (auto *window = qobject_cast<QQuickWindow *>(root)) {
            // The first `frameSwapped` can still carry a cleared back buffer, so wait a beat
            // before grabbing.
            QObject::connect(window, &QQuickWindow::frameSwapped, &app, [window, grabPath] {
                QTimer::singleShot(250, window, [window, grabPath] {
                    if (!window->grabWindow().save(grabPath)) {
                        qWarning("Could not write %s", qPrintable(grabPath));
                    }
                    QCoreApplication::quit();
                });
            }, Qt::QueuedConnection);
        }
    }

    if (parser.isSet(QStringLiteral("quit-after-first-frame"))) {
        if (auto *window = qobject_cast<QQuickWindow *>(root)) {
            QObject::connect(window, &QQuickWindow::frameSwapped, &app, &QGuiApplication::quit,
                             Qt::QueuedConnection);
        }
    }

    return app.exec();
}
