// SPDX-License-Identifier: AGPL-3.0-or-later
#include "LyricsLoader.h"

#include "LrcParser.h"
#include "TextDecoding.h"
#include "media/TagReader.h"
#include "media/TrackResolver.h"

#include <QFile>
#include <QFutureWatcher>
#include <QLoggingCategory>
#include <QtConcurrent>

Q_LOGGING_CATEGORY(lcLyrics, "lovelyrics.lyrics")

const QStringList LyricsLoader::DefaultSourceOrder = {
    QStringLiteral("sidecar"), QStringLiteral("embedded"), QStringLiteral("mpris")};

namespace {

constexpr qint64 MaxLyricFileBytes = 2 * 1024 * 1024;

LyricsDocument parseWithSource(const QString &text, const QString &source, const QString &path)
{
    LyricsDocument doc = LrcParser::parse(text);
    doc.source = source;
    doc.sourcePath = path;
    return doc;
}

} // namespace

LyricsLoader::LyricsLoader(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<LyricsDocument>();
}

void LyricsLoader::setSourceOrder(const QStringList &order)
{
    m_order = order.isEmpty() ? DefaultSourceOrder : order;
}

LyricsDocument LyricsLoader::loadNow(const TrackInfo &track, const QStringList &order)
{
    const QString path = TrackResolver::localPath(track.url);
    for (const QString &source : order) {
        if (source == u"sidecar") {
            const QString lrc = TrackResolver::findSidecarLyrics(path);
            if (lrc.isEmpty())
                continue;
            QFile f(lrc);
            if (f.size() > MaxLyricFileBytes || !f.open(QIODevice::ReadOnly))
                continue;
            QString encoding;
            const QString text = TextDecoding::decode(f.readAll(), &encoding);
            LyricsDocument doc = parseWithSource(text, source, lrc);
            qCDebug(lcLyrics) << "sidecar" << lrc << encoding << doc.lines.size() << "lines";
            if (!doc.isEmpty())
                return doc;
        } else if (source == u"embedded") {
            if (path.isEmpty() || !QFile::exists(path))
                continue;
            const QString text = TagReader::readLyrics(path);
            if (text.trimmed().isEmpty())
                continue;
            LyricsDocument doc = parseWithSource(text, source, path);
            if (!doc.isEmpty())
                return doc;
        } else if (source == u"mpris") {
            if (track.lyricsText.trimmed().isEmpty())
                continue;
            LyricsDocument doc = parseWithSource(track.lyricsText, source, {});
            if (!doc.isEmpty())
                return doc;
        }
    }
    return {};
}

void LyricsLoader::load(const TrackInfo &track)
{
    const quint64 id = ++m_request;
    if (m_pending++ == 0)
        emit loadingChanged(true);

    const QStringList order = m_order;
    auto *watcher = new QFutureWatcher<LyricsDocument>(this);
    connect(watcher, &QFutureWatcher<LyricsDocument>::finished, this, [this, watcher, id] {
        const LyricsDocument doc = watcher->result();
        watcher->deleteLater();
        if (--m_pending == 0)
            emit loadingChanged(false);
        if (id != m_request)
            return; // a newer track took over
        emit loaded(doc);
    });
    watcher->setFuture(QtConcurrent::run([track, order] { return loadNow(track, order); }));
}

void LyricsLoader::cancel()
{
    ++m_request;
}
