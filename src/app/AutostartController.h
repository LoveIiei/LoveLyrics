// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "mpris/MprisClient.h"

#include <QObject>

class Settings;
class WindowManager;

// Phase 4: start at login and show the window when the player starts playing.
//
// "Start at login" is an XDG autostart entry (~/.config/autostart/lovelyrics.desktop)
// running `lovelyrics --background`. In that resident mode, closing the window only
// hides it. The window is shown when playback starts (settings: off / window / full
// screen). If the user closes it during playback it stays closed until the player
// stops or quits, so pausing and resuming doesn't bring it back.
class AutostartController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool startAtLogin READ startAtLogin WRITE setStartAtLogin NOTIFY startAtLoginChanged)
    Q_PROPERTY(bool resident READ resident CONSTANT)

public:
    AutostartController(MprisClient *player, WindowManager *windows, Settings *settings, bool resident,
                        QObject *parent = nullptr);

    bool startAtLogin() const;
    void setStartAtLogin(bool enabled);
    bool resident() const { return m_resident; }

    static QString desktopFilePath();
    // Exec= line value: "lovelyrics" when installed in /usr, else this binary's path.
    static QString execCommand();
    static QString desktopFileContents();

signals:
    void startAtLoginChanged();

private:
    void onStatusChanged(MprisClient::PlaybackStatus status);
    void onConnectedChanged(bool connected);

    MprisClient *m_player;
    WindowManager *m_windows;
    Settings *m_settings;
    bool m_resident;
    bool m_dismissed = false; // closed by the user during this playback session
};
