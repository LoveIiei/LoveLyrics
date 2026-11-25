// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PaletteExtractor.h"
#include "mpris/MprisClient.h"

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QQuickImageProvider>
#include <QUrl>

// Loads the current track's cover on a worker thread and prepares:
//   full  - up to 1024 px, for display
//   blur  - small, heavily blurred copy for the ambient background (blurred once per track,
//           scaled up by the GPU, instead of blurring a full-screen texture every frame)
// plus the palette. Images are served to QML through CoverImageProvider ("image://cover/...").
class CoverLoader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source NOTIFY changed)
    Q_PROPERTY(QUrl blurredSource READ blurredSource NOTIFY changed)
    Q_PROPERTY(bool hasCover READ hasCover NOTIFY changed)
    Q_PROPERTY(QString origin READ origin NOTIFY changed) // "mpris", "embedded", "folder"
    Q_PROPERTY(QSize size READ size NOTIFY changed)

public:
    static constexpr int MaxCoverSize = 1024;
    static constexpr int BlurSize = 160;

    explicit CoverLoader(Palette *palette, QObject *parent = nullptr);

    void load(const TrackInfo &track);
    void clear();

    QUrl source() const { return m_source; }
    QUrl blurredSource() const { return m_blurredSource; }
    bool hasCover() const { return m_hasCover; }
    QString origin() const { return m_origin; }
    QSize size() const { return m_size; }

    // Thread-safe; used by the image provider. id = "<serial>/full" or "<serial>/blur".
    QImage image(const QString &id) const;

    struct Result
    {
        QImage full;
        QImage blurred;
        PaletteColors palette = PaletteColors::defaults();
        QString origin;
    };
    static Result loadNow(const TrackInfo &track); // synchronous, for tests
    static QImage blurred(const QImage &image, int size, int radius);

signals:
    void changed();

private:
    void apply(const Result &result);

    Palette *m_palette;
    quint64 m_request = 0;
    int m_serial = 0;
    QUrl m_source;
    QUrl m_blurredSource;
    bool m_hasCover = false;
    QString m_origin;
    QSize m_size;

    mutable QMutex m_mutex;
    QHash<QString, QImage> m_images; // current + previous track (for cross-fades)
};

class CoverImageProvider : public QQuickImageProvider
{
public:
    explicit CoverImageProvider(const CoverLoader *loader);
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    const CoverLoader *m_loader;
};
