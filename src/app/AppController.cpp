// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AppController.h"

#include "Settings.h"
#include "lyrics/LyricsCursor.h"
#include "lyrics/LyricsLoader.h"
#include "lyrics/LyricsModel.h"
#include "media/CoverLoader.h"
#include "media/PaletteExtractor.h"
#include "media/TrackResolver.h"
#include "mpris/MprisClient.h"
#include "playback/PlaybackClock.h"

#include <QQmlEngine>

AppController::AppController(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_player(new MprisClient(this))
    , m_clock(new PlaybackClock(this))
    , m_loader(new LyricsLoader(this))
    , m_lyrics(new LyricsModel(this))
    , m_cursor(new LyricsCursor(this))
    , m_palette(new Palette(this))
    , m_cover(new CoverLoader(m_palette, this))
{
    m_cursor->setModel(m_lyrics);

    // Settings -> backend
    m_player->setPlayerName(m_settings->playerName());
    connect(m_settings, &Settings::playerNameChanged, this,
            [this] { m_player->setPlayerName(m_settings->playerName()); });
    m_loader->setSourceOrder(m_settings->lyricSourceOrder());
    connect(m_settings, &Settings::lyricSourceOrderChanged, this,
            [this] { m_loader->setSourceOrder(m_settings->lyricSourceOrder()); });
    m_clock->setGlobalOffsetMs(m_settings->globalOffsetMs());
    connect(m_settings, &Settings::globalOffsetMsChanged, this,
            [this] { m_clock->setGlobalOffsetMs(m_settings->globalOffsetMs()); });

    // Player -> clock
    connect(m_player, &MprisClient::positionSynced, this, [this](qint64 ms, bool snap) {
        m_clock->sync(double(ms), snap ? PlaybackClock::SyncKind::Snap : PlaybackClock::SyncKind::Correct);
    });
    connect(m_player, &MprisClient::statusChanged, this, [this](MprisClient::PlaybackStatus s) {
        m_clock->setPlaying(s == MprisClient::Playing);
    });
    connect(m_player, &MprisClient::rateChanged, m_clock, &PlaybackClock::setRate);
    connect(m_player, &MprisClient::metadataChanged, this,
            [this] { m_clock->setDurationMs(std::max<qint64>(0, m_player->lengthMs())); });

    // Track -> lyrics, cover
    connect(m_player, &MprisClient::trackChanged, this, &AppController::onTrackChanged);
    connect(m_player, &MprisClient::artUrlChanged, this, [this](const QString &) {
        if (!m_viewActive)
            m_mediaPending = true;
        else if (!m_player->track().isEmpty())
            m_cover->load(m_player->track()); // art often arrives a moment after the track
    });
    connect(m_loader, &LyricsLoader::loaded, m_lyrics, &LyricsModel::setDocument);
    connect(m_loader, &LyricsLoader::loadingChanged, m_lyrics, &LyricsModel::setLoading);

    // Clock -> cursor, every frame
    connect(m_clock, &PlaybackClock::positionChanged, this,
            [this] { m_cursor->setPositionMs(m_clock->lyricsPositionMs()); });
}

AppController::~AppController() = default;

void AppController::onTrackChanged(const TrackInfo &track)
{
    // Never show the previous track's lyrics, even for a moment.
    m_loader->cancel();
    m_lyrics->clear();
    m_clock->reset();

    m_localPath = TrackResolver::localPath(track.url);
    if (!m_localPath.isEmpty())
        m_trackKey = m_localPath;
    else if (!track.url.isEmpty())
        m_trackKey = track.url;
    else if (!track.title.isEmpty())
        m_trackKey = track.artist() + QStringLiteral(" - ") + track.title;
    else
        m_trackKey.clear();
    m_clock->setTrackOffsetMs(m_settings->trackOffsetMs(m_trackKey));
    emit trackChanged();

    if (track.isEmpty()) {
        m_mediaPending = false;
        m_cover->clear();
        return;
    }
    if (!m_viewActive) {
        m_mediaPending = true; // loaded when the window shows
        return;
    }
    loadTrackMedia();
}

void AppController::loadTrackMedia()
{
    m_mediaPending = false;
    const TrackInfo &track = m_player->track();
    if (track.isEmpty())
        return;
    m_loader->load(track);
    m_cover->load(track);
}

void AppController::setViewActive(bool active)
{
    if (m_viewActive == active)
        return;
    m_viewActive = active;
    m_player->setPositionTracking(active);
    if (active && m_mediaPending)
        loadTrackMedia();
}

void AppController::adjustTrackOffset(int deltaMs)
{
    const int ms = int(m_clock->trackOffsetMs()) + deltaMs;
    m_clock->setTrackOffsetMs(ms);
    m_settings->setTrackOffsetMs(m_trackKey, ms);
}

void AppController::resetTrackOffset()
{
    m_clock->setTrackOffsetMs(0);
    m_settings->setTrackOffsetMs(m_trackKey, 0);
}

QString AppController::formatTime(double ms) const
{
    const qint64 total = qint64(std::max(0.0, ms)) / 1000;
    return QStringLiteral("%1:%2").arg(total / 60).arg(total % 60, 2, 10, QLatin1Char('0'));
}

void AppController::registerWithQml(QQmlEngine *engine)
{
    const char *uri = "LoveLyrics.Backend";
    qmlRegisterSingletonInstance(uri, 1, 0, "App", this);
    qmlRegisterSingletonInstance(uri, 1, 0, "Settings", m_settings);
    qmlRegisterSingletonInstance(uri, 1, 0, "Player", m_player);
    qmlRegisterSingletonInstance(uri, 1, 0, "Clock", m_clock);
    qmlRegisterSingletonInstance(uri, 1, 0, "Lyrics", m_lyrics);
    qmlRegisterSingletonInstance(uri, 1, 0, "Cursor", m_cursor);
    qmlRegisterSingletonInstance(uri, 1, 0, "Cover", m_cover);
    qmlRegisterSingletonInstance(uri, 1, 0, "Palette", m_palette);
    qmlRegisterUncreatableType<MprisClient>(uri, 1, 0, "MprisClient", QStringLiteral("Use the Player singleton"));
    engine->addImageProvider(QStringLiteral("cover"), new CoverImageProvider(m_cover));
}
