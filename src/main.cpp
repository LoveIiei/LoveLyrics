// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AppInfo.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(AppInfo::Id);
    QGuiApplication::setApplicationDisplayName(AppInfo::Name);
    QGuiApplication::setApplicationVersion(AppInfo::Version);
    QGuiApplication::setDesktopFileName(AppInfo::Id);

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("LoveLyrics", "Main");
    return app.exec();
}
