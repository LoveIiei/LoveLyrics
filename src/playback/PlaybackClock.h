// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <QElapsedTimer>
#include <QObject>

#include <functional>

// Interpolates the player's position between (rare) MPRIS position reads.
//
//   position = anchor + (now - anchorTime) * rate      while playing
//
// Small corrections (< SlewThresholdMs) are blended in over SlewDurationMs so text never
// stutters; seeks and big corrections snap. tick() is called once per rendered frame and
// publishes the new value to QML, so animations follow vsync.
class PlaybackClock : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double positionMs READ positionMs NOTIFY positionChanged)
    // positionMs plus the global and per-track lyric offsets: what lyrics should follow.
    Q_PROPERTY(double lyricsPositionMs READ lyricsPositionMs NOTIFY positionChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(double rate READ rate NOTIFY rateChanged)
    Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY durationChanged)
    Q_PROPERTY(qint64 globalOffsetMs READ globalOffsetMs WRITE setGlobalOffsetMs NOTIFY offsetChanged)
    Q_PROPERTY(qint64 trackOffsetMs READ trackOffsetMs WRITE setTrackOffsetMs NOTIFY offsetChanged)
    Q_PROPERTY(qint64 totalOffsetMs READ totalOffsetMs NOTIFY offsetChanged)
    // Last applied correction (ms, + means the clock was behind). For the debug page.
    Q_PROPERTY(double lastCorrectionMs READ lastCorrectionMs NOTIFY positionChanged)

public:
    static constexpr double SlewThresholdMs = 80.0;
    static constexpr double SlewDurationMs = 300.0;

    enum class SyncKind { Correct, Snap };
    using TimeSource = std::function<double()>; // monotonic milliseconds

    explicit PlaybackClock(QObject *parent = nullptr);

    void setTimeSource(TimeSource source); // for tests

    double positionMs() const { return m_published; }
    double lyricsPositionMs() const { return m_published + double(totalOffsetMs()); }
    bool playing() const { return m_playing; }
    double rate() const { return m_rate; }
    qint64 durationMs() const { return m_durationMs; }
    qint64 globalOffsetMs() const { return m_globalOffsetMs; }
    qint64 trackOffsetMs() const { return m_trackOffsetMs; }
    qint64 totalOffsetMs() const { return m_globalOffsetMs + m_trackOffsetMs; }
    double lastCorrectionMs() const { return m_lastCorrection; }

    // Position at the current time, without publishing it.
    double currentPositionMs() const { return positionAt(now()); }
    double positionAt(double nowMs) const;

    void sync(double positionMs, SyncKind kind);
    void setPlaying(bool playing);
    void setRate(double rate);
    void setDurationMs(qint64 ms);
    void setGlobalOffsetMs(qint64 ms);
    void setTrackOffsetMs(qint64 ms);
    void reset();

    // Recompute and publish the position. Call once per frame.
    Q_INVOKABLE void tick();

signals:
    void positionChanged();
    void playingChanged();
    void rateChanged();
    void durationChanged();
    void offsetChanged();

private:
    double now() const { return m_timeSource(); }
    double basePositionAt(double nowMs) const;
    double slewAt(double nowMs) const;
    void reanchor(double nowMs);

    TimeSource m_timeSource;
    QElapsedTimer m_timer;

    double m_anchorPos = 0;
    double m_anchorTime = 0;
    double m_rate = 1.0;
    bool m_playing = false;

    // Display offset that decays to zero after a small correction.
    double m_slewStart = 0;
    double m_slewTime = 0;

    double m_published = 0;
    double m_lastCorrection = 0;
    qint64 m_durationMs = 0;
    qint64 m_globalOffsetMs = 0;
    qint64 m_trackOffsetMs = 0;
};
