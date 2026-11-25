// SPDX-License-Identifier: AGPL-3.0-or-later
// Phase 4 integration test: the real app binary in --background mode against the fake
// player, single instance, auto-show / auto-hide, --quit, start-at-login file.
// ctest runs it inside dbus-run-session (private bus).
#include "app/AutostartController.h"
#include "app/Settings.h"
#include "app/WindowManager.h"
#include "mpris/MprisClient.h"

#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

class TestBackground : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    const QString m_player = QStringLiteral("lovelyricstest");

    QProcessEnvironment appEnv() const
    {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        env.insert(QStringLiteral("QT_QUICK_BACKEND"), QStringLiteral("software"));
        env.insert(QStringLiteral("QT_FORCE_STDERR_LOGGING"), QStringLiteral("1"));
        env.insert(QStringLiteral("QT_LOGGING_RULES"), QStringLiteral("lovelyrics.*=true\nlovelyrics.clock=false"));
        env.insert(QStringLiteral("LOVELYRICS_CONFIG"), m_dir.filePath(QStringLiteral("test.conf")));
        env.insert(QStringLiteral("XDG_CONFIG_HOME"), m_dir.path());
        return env;
    }

    static int runOnce(const QStringList &args, const QProcessEnvironment &env)
    {
        QProcess p;
        p.setProcessEnvironment(env);
        p.setProcessChannelMode(QProcess::ForwardedChannels);
        p.start(QStringLiteral(APP_BINARY), args);
        if (!p.waitForFinished(10000)) {
            p.kill();
            return -1;
        }
        return p.exitCode();
    }

    static qint64 rssKb(qint64 pid)
    {
        QFile f(QStringLiteral("/proc/%1/status").arg(pid));
        if (!f.open(QIODevice::ReadOnly))
            return -1;
        for (const QByteArray &line : f.readAll().split('\n'))
            if (line.startsWith("VmRSS:"))
                return line.mid(6).trimmed().split(' ').first().toLongLong();
        return -1;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        qputenv("XDG_CONFIG_HOME", m_dir.path().toUtf8()); // for startAtLogin()
        if (!QDBusConnection::sessionBus().isConnected())
            QSKIP("no session bus");
        QProcess check;
        check.start(QStringLiteral("python3"), {QStringLiteral("-c"), QStringLiteral("import dbus, gi")});
        check.waitForFinished();
        if (check.exitCode() != 0)
            QSKIP("python-dbus / python-gobject not installed");
    }

    void startAtLoginFile()
    {
        Settings settings(m_dir.filePath(QStringLiteral("unit.conf")));
        MprisClient player;
        WindowManager windows(nullptr, &settings, {});
        AutostartController autostart(&player, &windows, &settings, false);

        QVERIFY(AutostartController::desktopFilePath().startsWith(m_dir.path()));
        QVERIFY(!autostart.startAtLogin());
        autostart.setStartAtLogin(true);
        QVERIFY(autostart.startAtLogin());
        QFile f(AutostartController::desktopFilePath());
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString contents = QString::fromUtf8(f.readAll());
        QVERIFY2(contents.contains(QStringLiteral(" --background\n")), qPrintable(contents));
        QVERIFY(contents.contains(QStringLiteral("[Desktop Entry]")));
        f.close();
        autostart.setStartAtLogin(false);
        QVERIFY(!QFile::exists(AutostartController::desktopFilePath()));
    }

    void residentLifecycle()
    {
        // Settings for the app under test.
        {
            Settings s(m_dir.filePath(QStringLiteral("test.conf")));
            s.setPlayerName(m_player);
            s.setAutostartMode(QStringLiteral("window"));
            s.setHideWhenPlayerQuits(true);
            s.sync();
        }

        QProcess fake;
        fake.setProgram(QStringLiteral("python3"));
        fake.setArguments({QStringLiteral(FAKE_MPRIS), QStringLiteral("--bus-name"), m_player,
                           QStringLiteral(TEST_DATA_DIR "/library/01 Neon Rain.lrc")});
        fake.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        fake.start();
        QVERIFY(fake.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(fake.canReadLine(), 5000);
        QCOMPARE(fake.readLine().trimmed(), QByteArray("READY"));
        auto send = [&fake](const char *cmd) {
            fake.write(cmd);
            fake.write("\n");
            fake.waitForBytesWritten();
        };

        QProcess app;
        app.setProcessEnvironment(appEnv());
        app.setProcessChannelMode(QProcess::MergedChannels);
        QByteArray log;
        connect(&app, &QProcess::readyRead, this, [&] { log += app.readAll(); });
        app.start(QStringLiteral(APP_BINARY), {QStringLiteral("--background")});
        QVERIFY(app.waitForStarted());

        // Listening, no window yet.
        QTRY_VERIFY_WITH_TIMEOUT(log.contains("Player appeared"), 5000);
        QTest::qWait(500);
        QVERIFY2(!log.contains("Creating window"), log.constData());
        const qint64 idleRss = rssKb(app.processId());
        qInfo("background mode RSS: %lld KiB", idleRss);
        QVERIFY(idleRss > 0 && idleRss < 80 * 1024);

        // A second --background launch just exits.
        QCOMPARE(runOnce({QStringLiteral("--background")}, appEnv()), 0);
        QVERIFY(!log.contains("Activate requested"));

        // Playback starts: the window appears.
        send("p");
        QTRY_VERIFY_WITH_TIMEOUT(log.contains("Showing window"), 5000);
        QVERIFY(log.contains("Creating window"));

        // A plain second launch activates the first one.
        QCOMPARE(runOnce({}, appEnv()), 0);
        QTRY_VERIFY_WITH_TIMEOUT(log.contains("Activate requested"), 3000);

        // The player quits: the window hides (hideWhenPlayerQuits).
        send("release");
        QTRY_VERIFY_WITH_TIMEOUT(log.contains("Hiding window"), 5000);
        QCOMPARE(app.state(), QProcess::Running); // still resident

        // --quit stops it.
        QCOMPARE(runOnce({QStringLiteral("--quit")}, appEnv()), 0);
        QVERIFY(app.waitForFinished(5000));
        QCOMPARE(app.exitStatus(), QProcess::NormalExit);
        QCOMPARE(app.exitCode(), 0);

        // Nothing running now: --quit says so and succeeds.
        QCOMPARE(runOnce({QStringLiteral("--quit")}, appEnv()), 0);

        fake.kill();
        fake.waitForFinished();
    }
};

QTEST_GUILESS_MAIN(TestBackground)
#include "tst_background.moc"
