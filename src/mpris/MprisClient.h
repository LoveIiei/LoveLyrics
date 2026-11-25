// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QElapsedTimer>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

class QDBusServiceWatcher;

struct TrackInfo
{
    QString trackId;
    QString url;
    QString title;
    QStringList artists;
    QString album;
    QString artUrl;
    QString lyricsText; // xesam:asText
    qint64 lengthMs = -1;

    QString artist() const { return artists.join(QStringLiteral(", ")); }
    bool isEmpty() const { return trackId.isEmpty() && url.isEmpty() && title.isEmpty(); }
    // Same track (as opposed to a metadata update for the same track).
    bool sameTrack(const TrackInfo &o) const { return trackId == o.trackId && url == o.url; }
    bool operator==(const TrackInfo &o) const
    {
        return trackId == o.trackId && url == o.url && title == o.title && artists == o.artists
               && album == o.album && artUrl == o.artUrl && lyricsText == o.lyricsText && lengthMs == o.lengthMs;
    }
};
Q_DECLARE_METATYPE(TrackInfo)

// Follows one MPRIS player (default org.mpris.MediaPlayer2.strawberry) on the session bus.
// Copes with the player starting and quitting at any time.
class MprisClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString playerName READ playerName WRITE setPlayerName NOTIFY playerNameChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(PlaybackStatus status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString identity READ identity NOTIFY connectedChanged)
    Q_PROPERTY(QString trackId READ trackId NOTIFY metadataChanged)
    Q_PROPERTY(QString url READ url NOTIFY metadataChanged)
    Q_PROPERTY(QString title READ title NOTIFY metadataChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY metadataChanged)
    Q_PROPERTY(QString album READ album NOTIFY metadataChanged)
    Q_PROPERTY(QString artUrl READ artUrl NOTIFY metadataChanged)
    Q_PROPERTY(qint64 lengthMs READ lengthMs NOTIFY metadataChanged)
    Q_PROPERTY(double rate READ rate NOTIFY rateChanged)
    Q_PROPERTY(bool canControl READ canControl NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool canSeek READ canSeek NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool canGoPrevious READ canGoPrevious NOTIFY capabilitiesChanged)

public:
    enum PlaybackStatus { Stopped, Paused, Playing };
    Q_ENUM(PlaybackStatus)

    static constexpr int PollIntervalMs = 2000;

    explicit MprisClient(QObject *parent = nullptr);
    explicit MprisClient(const QDBusConnection &bus, QObject *parent = nullptr);
    ~MprisClient() override;

    // Short name ("strawberry") or a full bus name ("org.mpris.MediaPlayer2.strawberry").
    QString playerName() const { return m_playerName; }
    void setPlayerName(const QString &name);
    QString serviceName() const { return m_service; }

    bool connected() const { return m_connected; }
    PlaybackStatus status() const { return m_status; }
    QString identity() const { return m_identity; }
    const TrackInfo &track() const { return m_track; }
    QString trackId() const { return m_track.trackId; }
    QString url() const { return m_track.url; }
    QString title() const { return m_track.title; }
    QString artist() const { return m_track.artist(); }
    QString album() const { return m_track.album; }
    QString artUrl() const { return m_track.artUrl; }
    qint64 lengthMs() const { return m_track.lengthMs; }
    double rate() const { return m_rate; }
    bool canControl() const { return m_canControl; }
    bool canSeek() const { return m_canSeek; }
    bool canGoNext() const { return m_canGoNext; }
    bool canGoPrevious() const { return m_canGoPrevious; }

    static TrackInfo parseMetadata(const QVariantMap &metadata);
    static PlaybackStatus parseStatus(const QString &status);

public slots:
    void playPause();
    void next();
    void previous();
    void setPositionMs(qint64 ms);
    void seekByMs(qint64 deltaMs);
    // Ask the player for its position now (async); result comes as positionSynced().
    void requestPosition(bool snap = false);

signals:
    void playerNameChanged();
    void connectedChanged(bool connected);
    void statusChanged(MprisClient::PlaybackStatus status);
    void trackChanged(const TrackInfo &track); // a different track started
    void metadataChanged();                    // any metadata change, including the above
    void artUrlChanged(const QString &artUrl);
    void rateChanged(double rate);
    void capabilitiesChanged();
    // ms: player position. snap: a seek / track change, jump straight to it.
    void positionSynced(qint64 ms, bool snap);

private slots:
    void onPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated);
    void onSeeked(qlonglong positionUs);

private:
    void init();
    void subscribe();
    void unsubscribe();
    void attach();
    void detach();
    void fetchAll();
    void applyPlayerProperties(const QVariantMap &props);
    void setConnected(bool connected);
    void updatePolling();
    void callPlayer(const QString &method, const QVariantList &args = {});

    QDBusConnection m_bus;
    QDBusServiceWatcher *m_watcher = nullptr;
    QTimer m_pollTimer;
    QString m_playerName;
    QString m_service;
    bool m_subscribed = false;

    bool m_connected = false;
    QString m_identity;
    PlaybackStatus m_status = Stopped;
    TrackInfo m_track;
    double m_rate = 1.0;
    bool m_canControl = false;
    bool m_canSeek = false;
    bool m_canGoNext = false;
    bool m_canGoPrevious = false;

    // Replies older than the current generation (track change, seek) are dropped.
    quint64 m_generation = 0;
    QElapsedTimer m_clock;
};
