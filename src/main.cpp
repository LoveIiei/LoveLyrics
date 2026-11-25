// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AppInfo.h"
#include "app/AppController.h"
#include "app/Settings.h"
#include "mpris/MprisClient.h"

#include <QCommandLineParser>
#include <QFont>
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
        QStringLiteral("player"), QStringLiteral("MPRIS player to follow for this run (default: the saved setting, strawberry)."),
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
    // LOVELYRICS_CONFIG=/path/file.conf uses another settings file (tests, dev runs).
    const QString configOverride = qEnvironmentVariable("LOVELYRICS_CONFIG");
    Settings settings(configOverride.isEmpty() ? Settings::defaultPath() : configOverride);
    AppController controller(&settings);
    if (parser.isSet(playerOpt)) // this run only; the saved setting is left alone
        controller.player()->setPlayerName(parser.value(playerOpt));

    // Mixed CJK + Latin lines: when the chosen family lacks a glyph, try Noto Sans CJK SC
    // before fontconfig's own fallback (which may pick the JP variant for Chinese text).
    auto updateFontFallback = [&settings] {
        QFont::insertSubstitutions(settings.fontFamily(), {QStringLiteral("Noto Sans CJK SC")});
    };
    updateFontFallback();
    QObject::connect(&settings, &Settings::fontFamilyChanged, &app, updateFontFallback);

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
