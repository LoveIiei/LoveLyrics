// SPDX-License-Identifier: AGPL-3.0-or-later
#include "app/Settings.h"
#include "lyrics/LyricsLoader.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class TestSettings : public QObject
{
    Q_OBJECT

private slots:
    void defaultsAndPersistence()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("test.conf"));
        {
            Settings s(path);
            QCOMPARE(s.playerName(), u"strawberry");
            QCOMPARE(s.style(), u"Flow");
            QCOMPARE(s.lyricSourceOrder(), LyricsLoader::DefaultSourceOrder);
            QVERIFY(s.showTranslation());
            QCOMPARE(s.fullScreenScreen(), QString());

            QSignalSpy spy(&s, &Settings::styleChanged);
            s.setStyle(QStringLiteral("Kinetic"));
            s.setStyle(QStringLiteral("Kinetic")); // no second signal
            QCOMPARE(spy.count(), 1);
            s.setLyricSourceOrder({QStringLiteral("embedded"), QStringLiteral("sidecar"), QStringLiteral("mpris")});
            s.setFullScreenScreen(QStringLiteral("HDMI-A-1"));
            s.setFontScale(1.25);
            s.setWindowGeometry(QRect(10, 20, 1600, 900));
            s.setStyleValue(QStringLiteral("Flow"), QStringLiteral("blur"), 0.5);
            s.setStyleValue(QStringLiteral("Stage"), QStringLiteral("reflection"), false);
            s.setTrackOffsetMs(QStringLiteral("/music/a song.flac"), -300);
            s.sync();
        }
        Settings s(path);
        QCOMPARE(s.style(), u"Kinetic");
        QCOMPARE(s.lyricSourceOrder().first(), u"embedded");
        QCOMPARE(s.fullScreenScreen(), u"HDMI-A-1");
        QCOMPARE(s.fontScale(), 1.25);
        QCOMPARE(s.windowGeometry(), QRect(10, 20, 1600, 900));
        QCOMPARE(s.styleValue(QStringLiteral("Flow"), QStringLiteral("blur"), 1.0).toDouble(), 0.5);
        QCOMPARE(s.styleValue(QStringLiteral("Stage"), QStringLiteral("reflection"), true).toBool(), false);
        QCOMPARE(s.styleValue(QStringLiteral("Kinetic"), QStringLiteral("size"), 1.0).toDouble(), 1.0);
        QCOMPARE(s.trackOffsetMs(QStringLiteral("/music/a song.flac")), -300);
        QCOMPARE(s.trackOffsetMs(QStringLiteral("/music/other.flac")), 0);

        s.setTrackOffsetMs(QStringLiteral("/music/a song.flac"), 0); // zero removes the entry
        QCOMPARE(s.trackOffsetMs(QStringLiteral("/music/a song.flac")), 0);
    }
};

QTEST_GUILESS_MAIN(TestSettings)
#include "tst_settings.moc"
