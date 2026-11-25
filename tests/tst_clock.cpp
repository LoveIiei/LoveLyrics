// SPDX-License-Identifier: AGPL-3.0-or-later
#include "playback/PlaybackClock.h"

#include <QtTest>

class TestClock : public QObject
{
    Q_OBJECT

    double m_now = 0;
    std::unique_ptr<PlaybackClock> m_clock;

    void advance(double ms)
    {
        m_now += ms;
        m_clock->tick();
    }

private slots:
    void init()
    {
        m_now = 10000;
        m_clock = std::make_unique<PlaybackClock>();
        m_clock->setTimeSource([this] { return m_now; });
    }

    void interpolatesWhilePlaying()
    {
        m_clock->sync(1000, PlaybackClock::SyncKind::Snap);
        m_clock->setPlaying(true);
        advance(500);
        QCOMPARE(m_clock->positionMs(), 1500.0);
        advance(4.1667); // a 240 Hz frame
        QCOMPARE(m_clock->positionMs(), 1504.1667);
    }

    void pauseFreezes()
    {
        m_clock->sync(1000, PlaybackClock::SyncKind::Snap);
        m_clock->setPlaying(true);
        advance(1000);
        m_clock->setPlaying(false);
        advance(60000);
        QCOMPARE(m_clock->positionMs(), 2000.0);
        m_clock->setPlaying(true);
        advance(250);
        QCOMPARE(m_clock->positionMs(), 2250.0);
    }

    void rate()
    {
        m_clock->setPlaying(true);
        m_clock->setRate(2.0);
        advance(1000);
        QCOMPARE(m_clock->positionMs(), 2000.0);
    }

    void smallCorrectionSlews()
    {
        m_clock->sync(1000, PlaybackClock::SyncKind::Snap);
        m_clock->setPlaying(true);
        advance(1000); // 2000
        m_clock->sync(2050, PlaybackClock::SyncKind::Correct);
        QCOMPARE(m_clock->lastCorrectionMs(), 50.0);
        // No jump right away…
        QCOMPARE(m_clock->positionMs(), 2000.0);
        advance(150);
        QCOMPARE(m_clock->positionMs(), 2050.0 + 150 - 25);
        // …fully corrected after the slew.
        advance(150);
        QCOMPARE(m_clock->positionMs(), 2050.0 + 300);
        advance(1000);
        QCOMPARE(m_clock->positionMs(), 2050.0 + 1300);
    }

    void slewNeverGoesBackwards()
    {
        m_clock->sync(5000, PlaybackClock::SyncKind::Snap);
        m_clock->setPlaying(true);
        advance(100);
        m_clock->sync(5100 - 79, PlaybackClock::SyncKind::Correct); // clock was 79 ms ahead
        double last = m_clock->positionMs();
        for (int i = 0; i < 100; ++i) {
            advance(4.1667);
            QVERIFY(m_clock->positionMs() >= last);
            last = m_clock->positionMs();
        }
    }

    void bigCorrectionSnaps()
    {
        m_clock->sync(1000, PlaybackClock::SyncKind::Snap);
        m_clock->setPlaying(true);
        advance(1000);
        m_clock->sync(2500, PlaybackClock::SyncKind::Correct);
        QCOMPARE(m_clock->positionMs(), 2500.0);
    }

    void seekSnaps()
    {
        m_clock->sync(1000, PlaybackClock::SyncKind::Snap);
        m_clock->setPlaying(true);
        advance(1000);
        m_clock->sync(2010, PlaybackClock::SyncKind::Snap);
        QCOMPARE(m_clock->positionMs(), 2010.0);
    }

    void pausedCorrectionSnaps()
    {
        m_clock->sync(1000, PlaybackClock::SyncKind::Snap);
        m_clock->sync(1040, PlaybackClock::SyncKind::Correct);
        QCOMPARE(m_clock->positionMs(), 1040.0);
    }

    void offsets()
    {
        m_clock->sync(10000, PlaybackClock::SyncKind::Snap);
        m_clock->setGlobalOffsetMs(200);
        m_clock->setTrackOffsetMs(-500);
        QCOMPARE(m_clock->totalOffsetMs(), -300);
        QCOMPARE(m_clock->lyricsPositionMs(), 9700.0);
        QCOMPARE(m_clock->positionMs(), 10000.0);
    }

    void neverNegative()
    {
        m_clock->sync(0, PlaybackClock::SyncKind::Snap);
        m_clock->setPlaying(true);
        m_clock->sync(30, PlaybackClock::SyncKind::Correct);
        m_clock->sync(0, PlaybackClock::SyncKind::Correct);
        QVERIFY(m_clock->positionMs() >= 0);
    }

    void driftStaysSmallWithJitteryReads()
    {
        // Simulate 5 minutes with reads every 2 s that are off by up to ±40 ms.
        m_clock->sync(0, PlaybackClock::SyncKind::Snap);
        m_clock->setPlaying(true);
        double truth = 0;
        quint32 seed = 1;
        double maxErr = 0;
        for (int frame = 0; frame < 5 * 60 * 60; ++frame) { // 60 fps
            advance(1000.0 / 60);
            truth += 1000.0 / 60;
            if (frame % 120 == 0) {
                seed = seed * 1664525u + 1013904223u;
                const double noise = double(seed % 81) - 40;
                m_clock->sync(truth + noise, PlaybackClock::SyncKind::Correct);
            }
            maxErr = std::max(maxErr, std::abs(m_clock->positionMs() - truth));
        }
        QVERIFY2(maxErr < 50, qPrintable(QString::number(maxErr)));
    }
};

QTEST_GUILESS_MAIN(TestClock)
#include "tst_clock.moc"
