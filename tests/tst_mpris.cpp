// SPDX-License-Identifier: AGPL-3.0-or-later
// Integration test: MprisClient against tools/fake_mpris.py. ctest runs it inside
// dbus-run-session, so it uses a private bus and never sees a real player.
#include "mpris/MprisClient.h"

#include <QProcess>
#include <QSignalSpy>
#include <QtTest>

class TestMpris : public QObject
{
    Q_OBJECT

    const QString m_name = QStringLiteral("lovelyricstest");
    QProcess m_fake;

    void startFake()
    {
        m_fake.setProgram(QStringLiteral("python3"));
        m_fake.setArguments({QStringLiteral(FAKE_MPRIS), QStringLiteral("--bus-name"), m_name,
                             QStringLiteral("--autoplay"),
                             QStringLiteral(TEST_DATA_DIR "/library/01 Neon Rain.lrc"),
                             QStringLiteral(TEST_DATA_DIR "/library/02 Paper Moon.lrc")});
        m_fake.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        m_fake.start();
        QVERIFY(m_fake.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(m_fake.canReadLine() || m_fake.state() != QProcess::Running, 5000);
        QCOMPARE(m_fake.readLine().trimmed(), QByteArray("READY"));
    }

    void send(const char *cmd)
    {
        m_fake.write(cmd);
        m_fake.write("\n");
        m_fake.waitForBytesWritten();
    }

private slots:
    void initTestCase()
    {
        if (!QDBusConnection::sessionBus().isConnected())
            QSKIP("no session bus");
        QProcess check;
        check.start(QStringLiteral("python3"), {QStringLiteral("-c"), QStringLiteral("import dbus, gi")});
        check.waitForFinished();
        if (check.exitCode() != 0)
            QSKIP("python-dbus / python-gobject not installed");
    }

    void cleanupTestCase()
    {
        if (m_fake.state() == QProcess::Running) {
            m_fake.kill();
            m_fake.waitForFinished();
        }
    }

    void followsPlayer()
    {
        MprisClient client;
        client.setPlayerName(m_name);
        QVERIFY(!client.connected());

        QSignalSpy trackSpy(&client, &MprisClient::trackChanged);
        QSignalSpy posSpy(&client, &MprisClient::positionSynced);
        QSignalSpy statusSpy(&client, &MprisClient::statusChanged);

        // Player appears after us.
        startFake();
        QTRY_VERIFY_WITH_TIMEOUT(client.connected(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(client.title(), QStringLiteral("Neon Rain"), 5000);
        QCOMPARE(client.artist(), QStringLiteral("The Placeholders"));
        QVERIFY(client.url().endsWith(QStringLiteral("01%20Neon%20Rain.flac")));
        QVERIFY(client.artUrl().endsWith(QStringLiteral("cover.png")));
        QCOMPARE(client.lengthMs(), 100000);
        QTRY_COMPARE_WITH_TIMEOUT(client.status(), MprisClient::Playing, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!posSpy.isEmpty(), 3000);
        QCOMPARE(client.identity(), QStringLiteral("Fake MPRIS Player"));

        // Seek from the player: snaps.
        posSpy.clear();
        send("s 30");
        QTRY_VERIFY_WITH_TIMEOUT(!posSpy.isEmpty(), 3000);
        QVERIFY(qAbs(posSpy.last().at(0).toLongLong() - 30000) < 300);
        QVERIFY(posSpy.last().at(1).toBool());

        // Periodic reads while playing.
        posSpy.clear();
        QTRY_VERIFY_WITH_TIMEOUT(!posSpy.isEmpty(), MprisClient::PollIntervalMs + 1500);
        QVERIFY(!posSpy.last().at(1).toBool());

        // Pause.
        send("p");
        QTRY_COMPARE_WITH_TIMEOUT(client.status(), MprisClient::Paused, 3000);

        // Next track.
        trackSpy.clear();
        send("n");
        QTRY_VERIFY_WITH_TIMEOUT(!trackSpy.isEmpty(), 3000);
        QCOMPARE(client.title(), QStringLiteral("Paper Moon"));

        // Controls from our side.
        client.playPause();
        QTRY_COMPARE_WITH_TIMEOUT(client.status(), MprisClient::Playing, 3000);
        posSpy.clear();
        client.setPositionMs(10000);
        QTRY_VERIFY_WITH_TIMEOUT(!posSpy.isEmpty(), 3000);
        QVERIFY(qAbs(posSpy.last().at(0).toLongLong() - 10000) < 300);

        // Player quits and comes back.
        trackSpy.clear();
        send("release");
        QTRY_VERIFY_WITH_TIMEOUT(!client.connected(), 3000);
        QVERIFY(client.track().isEmpty());
        QCOMPARE(client.status(), MprisClient::Stopped);
        send("acquire");
        QTRY_VERIFY_WITH_TIMEOUT(client.connected(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.title(), QStringLiteral("Paper Moon"), 3000);

        // Player process dies.
        m_fake.kill();
        m_fake.waitForFinished();
        QTRY_VERIFY_WITH_TIMEOUT(!client.connected(), 3000);
        QVERIFY(statusSpy.count() >= 3);
    }

    void playerAlreadyRunning()
    {
        startFake();
        MprisClient client;
        client.setPlayerName(QStringLiteral("org.mpris.MediaPlayer2.") + m_name); // full name works too
        QTRY_VERIFY_WITH_TIMEOUT(client.connected(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.title(), QStringLiteral("Neon Rain"), 3000);
        m_fake.kill();
        m_fake.waitForFinished();
    }
};

QTEST_GUILESS_MAIN(TestMpris)
#include "tst_mpris.moc"
