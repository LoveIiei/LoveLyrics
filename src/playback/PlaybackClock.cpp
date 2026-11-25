// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PlaybackClock.h"

#include <QLoggingCategory>

#include <algorithm>
#include <cmath>

Q_LOGGING_CATEGORY(lcClock, "lovelyrics.clock")

PlaybackClock::PlaybackClock(QObject *parent)
    : QObject(parent)
{
    m_timer.start();
    m_timeSource = [this] { return double(m_timer.nsecsElapsed()) / 1e6; };
}

void PlaybackClock::setTimeSource(TimeSource source)
{
    m_timeSource = std::move(source);
    m_anchorTime = now();
    m_slewTime = m_anchorTime;
}

double PlaybackClock::basePositionAt(double nowMs) const
{
    if (!m_playing)
        return m_anchorPos;
    return m_anchorPos + (nowMs - m_anchorTime) * m_rate;
}

double PlaybackClock::slewAt(double nowMs) const
{
    if (m_slewStart == 0)
        return 0;
    const double t = (nowMs - m_slewTime) / SlewDurationMs;
    if (t >= 1.0)
        return 0;
    return m_slewStart * (1.0 - std::max(0.0, t));
}

double PlaybackClock::positionAt(double nowMs) const
{
    return std::max(0.0, basePositionAt(nowMs) + slewAt(nowMs));
}

void PlaybackClock::reanchor(double nowMs)
{
    m_anchorPos = positionAt(nowMs);
    m_anchorTime = nowMs;
    m_slewStart = 0;
}

void PlaybackClock::sync(double positionMs, SyncKind kind)
{
    const double t = now();
    const double predicted = positionAt(t);
    const double correction = positionMs - predicted;
    m_lastCorrection = correction;
    qCDebug(lcClock, "sync %s at %.1f ms: correction %+.1f ms", kind == SyncKind::Snap ? "snap" : "correct",
            positionMs, correction);

    if (kind == SyncKind::Correct && m_playing && std::abs(correction) < SlewThresholdMs) {
        // Keep the displayed value continuous and let the difference fade out.
        m_slewStart = predicted - positionMs;
        m_slewTime = t;
    } else {
        m_slewStart = 0;
    }
    m_anchorPos = positionMs;
    m_anchorTime = t;
    tick();
}

void PlaybackClock::setPlaying(bool playing)
{
    if (m_playing == playing)
        return;
    reanchor(now());
    m_playing = playing;
    emit playingChanged();
    tick();
}

void PlaybackClock::setRate(double rate)
{
    if (rate <= 0 || qFuzzyCompare(m_rate, rate))
        return;
    reanchor(now());
    m_rate = rate;
    emit rateChanged();
    tick();
}

void PlaybackClock::setDurationMs(qint64 ms)
{
    if (m_durationMs == ms)
        return;
    m_durationMs = ms;
    emit durationChanged();
}

void PlaybackClock::setGlobalOffsetMs(qint64 ms)
{
    if (m_globalOffsetMs == ms)
        return;
    m_globalOffsetMs = ms;
    emit offsetChanged();
    tick();
}

void PlaybackClock::setTrackOffsetMs(qint64 ms)
{
    if (m_trackOffsetMs == ms)
        return;
    m_trackOffsetMs = ms;
    emit offsetChanged();
    tick();
}

void PlaybackClock::reset()
{
    m_anchorPos = 0;
    m_anchorTime = now();
    m_slewStart = 0;
    m_lastCorrection = 0;
    tick();
}

void PlaybackClock::tick()
{
    m_published = positionAt(now());
    emit positionChanged();
}
