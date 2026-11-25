// SPDX-License-Identifier: AGPL-3.0-or-later
#include "CoverLoader.h"

#include "TagReader.h"
#include "TrackResolver.h"

#include <QFile>
#include <QFutureWatcher>
#include <QImageReader>
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QtConcurrent>

#include <vector>

Q_LOGGING_CATEGORY(lcCover, "lovelyrics.cover")

namespace {

QImage readImageFile(const QString &path)
{
    if (path.isEmpty())
        return {};
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize sz = reader.size();
    if (sz.isValid() && (sz.width() > CoverLoader::MaxCoverSize || sz.height() > CoverLoader::MaxCoverSize))
        reader.setScaledSize(sz.scaled(CoverLoader::MaxCoverSize, CoverLoader::MaxCoverSize, Qt::KeepAspectRatio));
    return reader.read();
}

QImage fitCover(const QImage &img)
{
    if (img.width() <= CoverLoader::MaxCoverSize && img.height() <= CoverLoader::MaxCoverSize)
        return img;
    return img.scaled(CoverLoader::MaxCoverSize, CoverLoader::MaxCoverSize, Qt::KeepAspectRatio,
                      Qt::SmoothTransformation);
}

// One horizontal box-blur pass with edge clamping, in place over rows of ARGB32 pixels.
void boxBlurRows(QImage &img, int radius)
{
    const int w = img.width();
    const int h = img.height();
    const int window = radius * 2 + 1;
    std::vector<QRgb> row(static_cast<size_t>(w));
    for (int y = 0; y < h; ++y) {
        auto *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        std::copy(line, line + w, row.begin());
        int sr = 0, sg = 0, sb = 0, sa = 0;
        auto px = [&](int x) { return row[size_t(std::clamp(x, 0, w - 1))]; };
        for (int x = -radius; x <= radius; ++x) {
            const QRgb p = px(x);
            sr += qRed(p), sg += qGreen(p), sb += qBlue(p), sa += qAlpha(p);
        }
        for (int x = 0; x < w; ++x) {
            line[x] = qRgba(sr / window, sg / window, sb / window, sa / window);
            const QRgb out = px(x - radius);
            const QRgb in = px(x + radius + 1);
            sr += qRed(in) - qRed(out);
            sg += qGreen(in) - qGreen(out);
            sb += qBlue(in) - qBlue(out);
            sa += qAlpha(in) - qAlpha(out);
        }
    }
}

QImage transposed(const QImage &img)
{
    return img.transformed(QTransform(0, 1, 1, 0, 0, 0));
}

} // namespace

CoverLoader::CoverLoader(Palette *palette, QObject *parent)
    : QObject(parent)
    , m_palette(palette)
{
}

QImage CoverLoader::blurred(const QImage &image, int size, int radius)
{
    if (image.isNull())
        return {};
    // Square crop-fill, like the background will show it.
    QImage img = image.convertToFormat(QImage::Format_ARGB32)
                     .scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    img = img.copy((img.width() - size) / 2, (img.height() - size) / 2, size, size);
    // Three box passes in each direction approximate a Gaussian.
    for (int pass = 0; pass < 3; ++pass)
        boxBlurRows(img, radius);
    img = transposed(img);
    for (int pass = 0; pass < 3; ++pass)
        boxBlurRows(img, radius);
    return transposed(img);
}

CoverLoader::Result CoverLoader::loadNow(const TrackInfo &track)
{
    Result r;
    const QString audioPath = TrackResolver::localPath(track.url);

    QImage img;
    if (const QString artPath = TrackResolver::localPath(track.artUrl); !artPath.isEmpty()) {
        img = readImageFile(artPath);
        if (!img.isNull())
            r.origin = QStringLiteral("mpris");
    }
    if (img.isNull() && !audioPath.isEmpty() && QFile::exists(audioPath)) {
        const QByteArray bytes = TagReader::readCover(audioPath);
        if (!bytes.isEmpty() && img.loadFromData(bytes))
            r.origin = QStringLiteral("embedded");
    }
    if (img.isNull()) {
        img = readImageFile(TrackResolver::findFolderCover(audioPath));
        if (!img.isNull())
            r.origin = QStringLiteral("folder");
    }
    if (img.isNull())
        return r;

    r.full = fitCover(img).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    r.blurred = blurred(r.full, BlurSize, 10);
    r.palette = PaletteExtractor::extract(r.full);
    return r;
}

void CoverLoader::load(const TrackInfo &track)
{
    const quint64 id = ++m_request;
    auto *watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher, id] {
        const Result r = watcher->result();
        watcher->deleteLater();
        if (id == m_request)
            apply(r);
    });
    watcher->setFuture(QtConcurrent::run([track] { return loadNow(track); }));
}

void CoverLoader::clear()
{
    ++m_request;
    apply({});
}

void CoverLoader::apply(const Result &result)
{
    const int serial = ++m_serial;
    {
        QMutexLocker lock(&m_mutex);
        // Keep the previous track's images so a cross-fade can still read them.
        const QString keep = QString::number(serial - 1) + QLatin1Char('/');
        for (auto it = m_images.begin(); it != m_images.end();) {
            if (it.key().startsWith(keep))
                ++it;
            else
                it = m_images.erase(it);
        }
        if (!result.full.isNull()) {
            m_images.insert(QStringLiteral("%1/full").arg(serial), result.full);
            m_images.insert(QStringLiteral("%1/blur").arg(serial), result.blurred);
        }
    }

    m_hasCover = !result.full.isNull();
    m_origin = result.origin;
    m_size = result.full.size();
    m_source = m_hasCover ? QUrl(QStringLiteral("image://cover/%1/full").arg(serial)) : QUrl();
    m_blurredSource = m_hasCover ? QUrl(QStringLiteral("image://cover/%1/blur").arg(serial)) : QUrl();
    qCDebug(lcCover) << "cover" << (m_hasCover ? m_origin : QStringLiteral("none")) << m_size;
    m_palette->setColors(result.palette);
    emit changed();
}

QImage CoverLoader::currentImage() const
{
    if (!m_hasCover)
        return {};
    QMutexLocker lock(&m_mutex);
    return m_images.value(QStringLiteral("%1/full").arg(m_serial));
}

QImage CoverLoader::image(const QString &id) const
{
    QMutexLocker lock(&m_mutex);
    return m_images.value(id);
}

CoverImageProvider::CoverImageProvider(const CoverLoader *loader)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_loader(loader)
{
}

QImage CoverImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QImage img = m_loader->image(id);
    if (size)
        *size = img.size();
    if (!img.isNull() && requestedSize.isValid() && requestedSize.width() > 0 && requestedSize.height() > 0
        && (requestedSize.width() < img.width() || requestedSize.height() < img.height()))
        img = img.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return img;
}
