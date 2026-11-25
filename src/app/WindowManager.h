// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickWindow>

#include <memory>

class AppController;
class FrameLimiter;
class QQmlApplicationEngine;
class Settings;

// Owns the QML engine and the main window. Both are created the first time the window
// is needed, so `--background` costs only the MPRIS listener until then.
class WindowManager : public QObject
{
    Q_OBJECT

public:
    struct Options
    {
        bool debugPage = false;
        bool settingsOpen = false;
    };

    WindowManager(AppController *controller, Settings *settings, Options options, QObject *parent = nullptr);
    ~WindowManager() override;

    QQuickWindow *window() const { return m_window; }
    bool isCreated() const { return m_window != nullptr; }
    bool isVisible() const;

    // Creates the window if needed, then shows it (full screen on the chosen monitor if
    // asked), raises it and asks for focus. activationToken: from the launching process.
    void show(bool fullScreen, const QString &activationToken = {});
    // Hides the window (not a user action: no userHid()).
    void hide();
    // Saves window geometry/screen, if there is a window.
    void saveState();

signals:
    void windowCreated(QQuickWindow *window);
    // The user closed/hid the window.
    void userHid();
    // The window became visible / hidden (for any reason).
    void visibleChanged(bool visible);

private:
    bool create();

    AppController *m_controller;
    Settings *m_settings;
    Options m_options;
    std::unique_ptr<QQmlApplicationEngine> m_engine;
    QPointer<QQuickWindow> m_window;
    FrameLimiter *m_limiter = nullptr;
    bool m_programmaticHide = false;
};
