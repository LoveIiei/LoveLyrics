// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AppInfo.h"
#include "app/AppController.h"
#include "app/AutostartController.h"
#include "app/Settings.h"
#include "app/SingleInstance.h"
#include "app/WindowManager.h"
#include "mpris/MprisClient.h"

#include <QCommandLineParser>
#include <QFont>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QTimer>

#include <cstdio>

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
    const QCommandLineOption backgroundOpt(
        QStringLiteral("background"),
        QStringLiteral("Start without a window and show it when the player starts playing (used at login)."));
    const QCommandLineOption quitOpt(QStringLiteral("quit"), QStringLiteral("Quit the running instance."));
    const QCommandLineOption playerOpt(
        QStringLiteral("player"),
        QStringLiteral("MPRIS player to follow for this run (default: the saved setting, strawberry)."),
        QStringLiteral("name"));
    const QCommandLineOption debugOpt(QStringLiteral("debug"), QStringLiteral("Start on the debug page."));
    const QCommandLineOption settingsOpt(QStringLiteral("settings"), QStringLiteral("Open the settings panel."));
    const QCommandLineOption grabOpt(QStringLiteral("grab"),
                                     QStringLiteral("Dev: save a screenshot of the window to <file> and quit."),
                                     QStringLiteral("file"));
    const QCommandLineOption grabAfterOpt(QStringLiteral("grab-after"),
                                          QStringLiteral("Dev: delay before --grab, in ms (default 3000)."),
                                          QStringLiteral("ms"), QStringLiteral("3000"));
    parser.addOptions({backgroundOpt, quitOpt, playerOpt, debugOpt, settingsOpt, grabOpt, grabAfterOpt});
    parser.process(app);

    // ---- one instance per session -------------------------------------------------------
    SingleInstance instance;
    if (!instance.registerPrimary()) {
        if (parser.isSet(quitOpt))
            return SingleInstance::callRunning(QStringLiteral("Quit")) ? 0 : 1;
        if (parser.isSet(backgroundOpt))
            return 0; // already running; nothing to show
        // Hand our activation token to the running copy so it may take focus.
        const QString token = qEnvironmentVariable("XDG_ACTIVATION_TOKEN");
        return SingleInstance::callRunning(QStringLiteral("Activate"), {token}) ? 0 : 1;
    }
    if (parser.isSet(quitOpt)) {
        std::fputs("lovelyrics is not running.\n", stderr);
        return 0;
    }
    qunsetenv("XDG_ACTIVATION_TOKEN"); // only valid for our own first window

    const bool resident = parser.isSet(backgroundOpt);
    // Resident: closing the window only hides it; quit with Ctrl+Q, the settings panel
    // or `lovelyrics --quit`.
    QGuiApplication::setQuitOnLastWindowClosed(!resident);

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

    WindowManager windows(&controller, &settings,
                          {.debugPage = parser.isSet(debugOpt), .settingsOpen = parser.isSet(settingsOpt)});
    // Nothing on screen yet: don't load media or poll the position until the window shows.
    controller.setViewActive(false);
    QObject::connect(&windows, &WindowManager::visibleChanged, &controller, &AppController::setViewActive);
    AutostartController autostart(controller.player(), &windows, &settings, resident);
    qmlRegisterSingletonInstance("LoveLyrics.Backend", 1, 0, "Autostart", &autostart);

    QObject::connect(&instance, &SingleInstance::activateRequested, &app,
                     [&windows](const QString &token) { windows.show(false, token); });
    QObject::connect(&instance, &SingleInstance::quitRequested, &app, &QCoreApplication::quit, Qt::QueuedConnection);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &windows, &WindowManager::saveState);

    if (!resident || parser.isSet(grabOpt))
        windows.show(settings.fullScreen());

    if (parser.isSet(grabOpt)) {
        const QString file = parser.value(grabOpt);
        QTimer::singleShot(parser.value(grabAfterOpt).toInt(), &app, [&windows, file] {
            const bool ok = windows.window() && windows.window()->grabWindow().save(file);
            qInfo("grab %s: %s", qPrintable(file), ok ? "ok" : "failed");
            QCoreApplication::exit(ok ? 0 : 1);
        });
    }
    return app.exec();
}
