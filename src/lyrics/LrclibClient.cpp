// SPDX-License-Identifier: AGPL-3.0-or-later
#include "LrclibClient.h"

#include "AppInfo.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrlQuery>

Q_LOGGING_CATEGORY(lcLrclib, "lovelyrics.lrclib")

namespace {

constexpr int TimeoutMs = 10000;

// The lyrics in an LRCLIB record: synced if it has them, else plain; "" for none.
QString lyricsOf(const QJsonObject &o, bool *synced)
{
    const QString s = o.value(QStringLiteral("syncedLyrics")).toString();
    if (!s.trimmed().isEmpty()) {
        *synced = true;
        return s;
    }
    *synced = false;
    return o.value(QStringLiteral("plainLyrics")).toString();
}

} // namespace

LrclibClient::LrclibClient(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
}

QString LrclibClient::key(const TrackInfo &t)
{
    return t.artist().toLower().trimmed() + QLatin1Char('|') + t.title.toLower().trimmed() + QLatin1Char('|')
           + t.album.toLower().trimmed() + QLatin1Char('|') + QString::number(t.lengthMs > 0 ? t.lengthMs / 1000 : 0);
}

QString LrclibClient::cacheDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/")
           + QString::fromLatin1(AppInfo::Id) + QStringLiteral("/lyrics");
}

QString LrclibClient::cachePath(const TrackInfo &t)
{
    if (t.title.trimmed().isEmpty())
        return {};
    const QByteArray hash = QCryptographicHash::hash(key(t).toUtf8(), QCryptographicHash::Sha1).toHex();
    return cacheDirectory() + QLatin1Char('/') + QString::fromLatin1(hash.left(24)) + QStringLiteral(".lrc");
}

bool LrclibClient::writeCache(const TrackInfo &t, const QString &lrc)
{
    const QString path = cachePath(t);
    if (path.isEmpty())
        return false;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(lrc.toUtf8());
    return f.commit();
}

QNetworkReply *LrclibClient::request(const QString &path, const QList<std::pair<QString, QString>> &query)
{
    QUrl url = m_base;
    url.setPath(url.path().chopped(url.path().endsWith(QLatin1Char('/')) ? 1 : 0) + path);
    QUrlQuery q;
    for (const auto &[k, v] : query)
        if (!v.isEmpty())
            q.addQueryItem(k, v);
    url.setQuery(q);
    QNetworkRequest req(url);
    // LRCLIB asks clients to identify themselves; this names the app only.
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("%1/%2").arg(QString::fromLatin1(AppInfo::Name).remove(QLatin1Char(' ')),
                                              QString::fromLatin1(AppInfo::Version)));
    req.setTransferTimeout(TimeoutMs);
    return m_net->get(req);
}

void LrclibClient::lookup(const TrackInfo &track, quint64 id)
{
    if (track.title.trimmed().isEmpty()) {
        emit notFound(id, QStringLiteral("no title"));
        return;
    }
    if (m_misses.contains(key(track))) {
        emit notFound(id, QStringLiteral("not found earlier"));
        return;
    }
    get(track, id);
}

void LrclibClient::get(const TrackInfo &track, quint64 id)
{
    QNetworkReply *reply = request(QStringLiteral("/api/get"),
                                   {{QStringLiteral("track_name"), track.title},
                                    {QStringLiteral("artist_name"), track.artist()},
                                    {QStringLiteral("album_name"), track.album},
                                    {QStringLiteral("duration"),
                                     track.lengthMs > 0 ? QString::number(qRound(track.lengthMs / 1000.0)) : QString()}});
    connect(reply, &QNetworkReply::finished, this, [this, reply, track, id] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError && status == 200) {
            const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
            bool synced = false;
            const QString text = lyricsOf(o, &synced);
            if (!text.trimmed().isEmpty()) {
                qCInfo(lcLrclib) << "found" << track.artist() << "-" << track.title << (synced ? "synced" : "plain");
                emit found(id, text, synced);
                return;
            }
        }
        if (reply->error() != QNetworkReply::NoError && status != 404) {
            // Network trouble: don't remember it as a miss, try again next time.
            qCInfo(lcLrclib) << "lookup failed:" << reply->errorString();
            emit notFound(id, reply->errorString());
            return;
        }
        search(track, id);
    });
}

void LrclibClient::search(const TrackInfo &track, quint64 id)
{
    QNetworkReply *reply = request(QStringLiteral("/api/search"), {{QStringLiteral("track_name"), track.title},
                                                                   {QStringLiteral("artist_name"), track.artist()}});
    connect(reply, &QNetworkReply::finished, this, [this, reply, track, id] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit notFound(id, reply->errorString());
            return;
        }
        const QJsonArray results = QJsonDocument::fromJson(reply->readAll()).array();
        // Best: same artist, closest length, synced lyrics preferred.
        double bestScore = -1e9;
        QString bestText;
        bool bestSynced = false;
        const QString artist = track.artist().toLower();
        for (const QJsonValue &v : results) {
            const QJsonObject o = v.toObject();
            bool synced = false;
            const QString text = lyricsOf(o, &synced);
            if (text.trimmed().isEmpty())
                continue;
            double score = synced ? 10 : 0;
            const QString a = o.value(QStringLiteral("artistName")).toString().toLower();
            if (!artist.isEmpty() && (a == artist || a.contains(artist) || artist.contains(a)))
                score += 20;
            if (track.lengthMs > 0) {
                const double diff = std::abs(o.value(QStringLiteral("duration")).toDouble() - track.lengthMs / 1000.0);
                if (diff > 4)
                    continue; // a different recording
                score -= diff * 2;
            }
            if (score > bestScore) {
                bestScore = score;
                bestText = text;
                bestSynced = synced;
            }
        }
        if (bestText.isEmpty()) {
            m_misses.insert(key(track));
            qCInfo(lcLrclib) << "no lyrics for" << track.artist() << "-" << track.title;
            emit notFound(id, QStringLiteral("not found"));
            return;
        }
        qCInfo(lcLrclib) << "found by search" << track.artist() << "-" << track.title << (bestSynced ? "synced" : "plain");
        emit found(id, bestText, bestSynced);
    });
}
