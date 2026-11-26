// SPDX-License-Identifier: AGPL-3.0-or-later
// LrclibClient against a tiny local HTTP server (no real network).
#include "lyrics/LrclibClient.h"
#include "lyrics/LyricsLoader.h"

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

class FakeLrclib : public QObject
{
    Q_OBJECT
public:
    QTcpServer server;
    QStringList paths;
    // path prefix -> (status, body)
    QHash<QString, std::pair<int, QByteArray>> routes;

    FakeLrclib()
    {
        server.listen(QHostAddress::LocalHost);
        connect(&server, &QTcpServer::newConnection, this, [this] {
            QTcpSocket *s = server.nextPendingConnection();
            connect(s, &QTcpSocket::readyRead, s, [this, s] {
                const QByteArray req = s->readAll();
                const QString path = QString::fromLatin1(req.split(' ').value(1));
                paths << path;
                std::pair<int, QByteArray> r{404, "{\"message\":\"not found\"}"};
                for (auto it = routes.cbegin(); it != routes.cend(); ++it)
                    if (path.startsWith(it.key()))
                        r = it.value();
                s->write("HTTP/1.1 " + QByteArray::number(r.first) + (r.first == 200 ? " OK" : " Not Found")
                         + "\r\nContent-Type: application/json\r\nContent-Length: " + QByteArray::number(r.second.size())
                         + "\r\nConnection: close\r\n\r\n" + r.second);
                s->disconnectFromHost();
            });
        });
    }
    QUrl url() const { return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())); }
};

class TestLrclib : public QObject
{
    Q_OBJECT

    static TrackInfo track()
    {
        TrackInfo t;
        t.title = QStringLiteral("Neon Rain");
        t.artists = {QStringLiteral("The Placeholders")};
        t.album = QStringLiteral("Test Pressings");
        t.lengthMs = 100000;
        t.url = QStringLiteral("file:///nonexistent/Neon Rain.flac");
        return t;
    }

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void cleanup() { QDir(LrclibClient::cacheDirectory()).removeRecursively(); }

    void exactMatch()
    {
        FakeLrclib fake;
        fake.routes[QStringLiteral("/api/get")] = {200, R"({"syncedLyrics":"[00:01.00]hello","plainLyrics":"hello"})"};
        LrclibClient client;
        client.setBaseUrl(fake.url());
        QSignalSpy found(&client, &LrclibClient::found);
        client.lookup(track(), 7);
        QVERIFY(found.wait(5000));
        QCOMPARE(found.first().at(0).toULongLong(), 7u);
        QCOMPARE(found.first().at(1).toString(), QStringLiteral("[00:01.00]hello"));
        QVERIFY(found.first().at(2).toBool());
        QVERIFY(fake.paths.first().contains(QStringLiteral("track_name=Neon%20Rain")));
        QVERIFY(fake.paths.first().contains(QStringLiteral("duration=100")));
    }

    void searchFallbackPicksBest()
    {
        FakeLrclib fake;
        // /api/get misses (404); search has a wrong-length match, a plain one and a synced one.
        fake.routes[QStringLiteral("/api/search")] = {200, R"([
            {"artistName":"The Placeholders","duration":240,"syncedLyrics":"[00:01.00]wrong length"},
            {"artistName":"The Placeholders","duration":100,"plainLyrics":"plain words"},
            {"artistName":"The Placeholders","duration":101,"syncedLyrics":"[00:01.00]right one"}
        ])"};
        LrclibClient client;
        client.setBaseUrl(fake.url());
        QSignalSpy found(&client, &LrclibClient::found);
        client.lookup(track(), 1);
        QVERIFY(found.wait(5000));
        QCOMPARE(found.first().at(1).toString(), QStringLiteral("[00:01.00]right one"));
        QCOMPARE(fake.paths.size(), 2);
    }

    void missIsRemembered()
    {
        FakeLrclib fake;
        fake.routes[QStringLiteral("/api/search")] = {200, "[]"};
        LrclibClient client;
        client.setBaseUrl(fake.url());
        QSignalSpy missed(&client, &LrclibClient::notFound);
        client.lookup(track(), 1);
        QVERIFY(missed.wait(5000));
        const qsizetype requests = fake.paths.size();
        client.lookup(track(), 2); // answered from memory, no request
        QCOMPARE(missed.size(), 2);
        QCOMPARE(fake.paths.size(), requests);
    }

    void cacheIsALyricsSource()
    {
        const TrackInfo t = track();
        QVERIFY(LrclibClient::writeCache(t, QStringLiteral("[00:01.00]from cache\n[00:03.00]second")));
        const auto doc = LyricsLoader::loadNow(t, {QStringLiteral("sidecar"), QStringLiteral("cache")});
        QCOMPARE(doc.source, QStringLiteral("lrclib"));
        QVERIFY(doc.synced);
        // Another track does not see it.
        TrackInfo other = t;
        other.title = QStringLiteral("Other");
        QVERIFY(LyricsLoader::loadNow(other, {QStringLiteral("cache")}).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestLrclib)
#include "tst_lrclib.moc"
