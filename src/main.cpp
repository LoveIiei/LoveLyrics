// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AppInfo.h"
#include "app/AppController.h"
#include "app/Settings.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(AppInfo::Id);
    QGuiApplication::setApplicationName(AppInfo::Id);
    QGuiApplication::setApplicationDisplayName(AppInfo::Name);
    QGuiApplication::setApplicationVersion(AppInfo::Version);
    QGuiApplication::setDesktopFileName(AppInfo::Id);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Immersive synced lyrics for Strawberry (MPRIS)."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption playerOpt(
        QStringLiteral("player"), QStringLiteral("MPRIS player to follow (default: strawberry). Saved."),
        QStringLiteral("name"));
    const QCommandLineOption debugOpt(QStringLiteral("debug"), QStringLiteral("Start on the debug page."));
    const QCommandLineOption grabOpt(QStringLiteral("grab"),
                                     QStringLiteral("Dev: save a screenshot of the window to <file> and quit."),
                                     QStringLiteral("file"));
    const QCommandLineOption grabAfterOpt(QStringLiteral("grab-after"),
                                          QStringLiteral("Dev: delay before --grab, in ms (default 3000)."),
                                          QStringLiteral("ms"), QStringLiteral("3000"));
    parser.addOption(playerOpt);
    parser.addOption(debugOpt);
    parser.addOption(grabOpt);
    parser.addOption(grabAfterOpt);
    parser.process(app);

    // Destroyed after the engine (declared first), since QML and the image provider use it.
    Settings settings;
    if (parser.isSet(playerOpt))
        settings.setPlayerName(parser.value(playerOpt));
    AppController controller(&settings);

    QQmlApplicationEngine engine;
    controller.registerWithQml(&engine);
    engine.setInitialProperties({{QStringLiteral("startOnDebugPage"), parser.isSet(debugOpt)}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("LoveLyrics", "Main");

    if (parser.isSet(grabOpt)) {
        const QString file = parser.value(grabOpt);
        QTimer::singleShot(parser.value(grabAfterOpt).toInt(), &app, [&engine, file] {
            const auto roots = engine.rootObjects();
            auto *window = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(roots.first());
            const bool ok = window && window->grabWindow().save(file);
            qInfo("grab %s: %s", qPrintable(file), ok ? "ok" : "failed");
            QCoreApplication::exit(ok ? 0 : 1);
        });
    }
    return app.exec();
}
