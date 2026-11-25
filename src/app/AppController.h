// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <QObject>

class QQmlEngine;
class Settings;
class MprisClient;
class PlaybackClock;
class LyricsLoader;
class LyricsModel;
class LyricsCursor;
class CoverLoader;
class Palette;
struct TrackInfo;

// Owns the backend objects and connects them:
//   MprisClient -> PlaybackClock -> LyricsCursor
//   MprisClient -> LyricsLoader -> LyricsModel
//   MprisClient -> CoverLoader  -> Palette
class AppController : public QObject
{
    Q_OBJECT
    // Key used for the per-track offset: local path, else url, else "artist - title".
    Q_PROPERTY(QString trackKey READ trackKey NOTIFY trackChanged)
    Q_PROPERTY(QString localPath READ localPath NOTIFY trackChanged)

public:
    explicit AppController(Settings *settings, QObject *parent = nullptr);
    ~AppController() override;

    Settings *settings() const { return m_settings; }
    MprisClient *player() const { return m_player; }
    PlaybackClock *clock() const { return m_clock; }
    LyricsModel *lyrics() const { return m_lyrics; }
    LyricsCursor *cursor() const { return m_cursor; }
    CoverLoader *cover() const { return m_cover; }
    Palette *palette() const { return m_palette; }

    QString trackKey() const { return m_trackKey; }
    QString localPath() const { return m_localPath; }

    // Registers the singletons (App, Player, Clock, Lyrics, Cursor, Cover, Palette, Settings)
    // under "import LoveLyrics.Backend" and the image://cover provider.
    void registerWithQml(QQmlEngine *engine);

    // Lyric offset for the current track (saved per track). + shows lyrics later.
    Q_INVOKABLE void adjustTrackOffset(int deltaMs);
    Q_INVOKABLE void resetTrackOffset();
    Q_INVOKABLE QString formatTime(double ms) const;

signals:
    void trackChanged();

private:
    void onTrackChanged(const TrackInfo &track);

    Settings *m_settings;
    MprisClient *m_player;
    PlaybackClock *m_clock;
    LyricsLoader *m_loader;
    LyricsModel *m_lyrics;
    LyricsCursor *m_cursor;
    Palette *m_palette;
    CoverLoader *m_cover;
    QString m_trackKey;
    QString m_localPath;
};
