// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "mpris/MprisClient.h"

#include <QObject>
#include <QSet>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

// Looks up lyrics on LRCLIB (https://lrclib.net), a free, open lyrics database. Opt-in:
// it sends the artist, title, album and length of the track. Results are cached in
// ~/.cache/lovelyrics/lyrics/ (see cachePath), so a song is only looked up once.
//
//   1. GET /api/get?track_name&artist_name&album_name&duration   (exact match)
//   2. GET /api/search?track_name&artist_name                     (fallback: best match by
//      artist and duration, synced lyrics preferred)
class LrclibClient : public QObject
{
    Q_OBJECT

public:
    explicit LrclibClient(QObject *parent = nullptr);

    void setBaseUrl(const QUrl &url) { m_base = url; } // for tests
    QUrl baseUrl() const { return m_base; }

    // Async; emits found() or notFound() with the same id. A track already looked up
    // without success in this session is not asked again.
    void lookup(const TrackInfo &track, quint64 id);

    // Where a track's downloaded lyrics are cached ("" if the track has no title).
    static QString cachePath(const TrackInfo &track);
    static QString cacheDirectory();
    static bool writeCache(const TrackInfo &track, const QString &lrc);

signals:
    void found(quint64 id, const QString &lrcText, bool synced);
    void notFound(quint64 id, const QString &reason);

private:
    void get(const TrackInfo &track, quint64 id);
    void search(const TrackInfo &track, quint64 id);
    QNetworkReply *request(const QString &path, const QList<std::pair<QString, QString>> &query);
    static QString key(const TrackInfo &track);

    QNetworkAccessManager *m_net;
    QUrl m_base = QUrl(QStringLiteral("https://lrclib.net"));
    QSet<QString> m_misses;
};
