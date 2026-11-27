// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "mpris/MprisClient.h"

#include <QObject>

class Settings;
class WindowManager;

// Phase 4: start at login and show the window when the player starts playing.
//
// "Start at login" is an XDG autostart entry (~/.config/autostart/lovelyrics.desktop)
// running `lovelyrics --background`. The app is resident when started that way, or
// whenever start at login is on (so turning it on takes effect without logging out):
// closing the window then only hides it. The window is shown when playback starts (settings: off / window / full
// screen). If the user closes it during playback it stays closed until the player
// stops or quits, so pausing and resuming doesn't bring it back.
class AutostartController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool startAtLogin READ startAtLogin WRITE setStartAtLogin NOTIFY startAtLoginChanged)
    Q_PROPERTY(bool resident READ resident NOTIFY residentChanged)

public:
    AutostartController(MprisClient *player, WindowManager *windows, Settings *settings, bool startedInBackground,
                        QObject *parent = nullptr);

    bool startAtLogin() const;
    void setStartAtLogin(bool enabled);
    bool resident() const { return m_startedInBackground || startAtLogin(); }

    static QString desktopFilePath();
    // Exec= line value: "lovelyrics" when installed in /usr, else this binary's path.
    static QString execCommand();
    static QString desktopFileContents();

signals:
    void startAtLoginChanged();
    void residentChanged();

private:
    void onStatusChanged(MprisClient::PlaybackStatus status);
    void onConnectedChanged(bool connected);

    MprisClient *m_player;
    WindowManager *m_windows;
    Settings *m_settings;
    bool m_startedInBackground;
    bool m_dismissed = false; // closed by the user during this playback session
};
