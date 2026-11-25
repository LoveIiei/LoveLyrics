// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "CardRenderer.h"

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QQuickImageProvider>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

class CoverLoader;
class LyricsModel;
class LyricsCursor;
class MprisClient;
class PlaybackClock;
class Settings;

// The lyric card editor's model. QML reads `state` (template, ratio, style, mode, blur,
// depthX, font, scale, align, showTranslation, title, artist, seedIndex, date / note /
// progress extras) and changes it with set(key, value); `lines` lists the song's lines with
// their selected / highlighted flags. Every change re-renders the preview on a worker thread.
class CardController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)
    Q_PROPERTY(QVariantList lines READ lines NOTIFY linesChanged)
    Q_PROPERTY(QVariantList swatches READ swatches NOTIFY analysisChanged)
    Q_PROPERTY(QVariantList templates READ templates CONSTANT)
    Q_PROPERTY(QVariantMap templateInfo READ templateInfo NOTIFY stateChanged)
    Q_PROPERTY(QUrl previewSource READ previewSource NOTIFY previewChanged)
    Q_PROPERTY(double previewAspect READ previewAspect NOTIFY previewChanged) // height / width
    Q_PROPERTY(QVariantList hits READ hits NOTIFY previewChanged) // [{line, x, y, w, h}] 0..1
    Q_PROPERTY(int dropped READ dropped NOTIFY previewChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    CardController(Settings *settings, MprisClient *player, PlaybackClock *clock, LyricsModel *lyrics,
                   LyricsCursor *cursor, CoverLoader *cover, QObject *parent = nullptr);

    QVariantMap state() const { return m_state; }
    QVariantList lines() const;
    QVariantList swatches() const;
    QVariantList templates() const;
    QVariantMap templateInfo() const;
    QUrl previewSource() const { return m_previewSource; }
    double previewAspect() const { return m_previewAspect; }
    QVariantList hits() const { return m_hits; }
    int dropped() const { return m_dropped; }
    bool busy() const { return m_busy; }

    // Fill the editor from what is playing: title, artist, cover, and the current line
    // with the next few selected.
    Q_INVOKABLE void open();
    // While open, a new song (or lyrics arriving late) refills the editor.
    Q_INVOKABLE void close();
    Q_INVOKABLE void set(const QString &key, const QVariant &value);
    Q_INVOKABLE void toggleLine(int lineIndex);
    Q_INVOKABLE void toggleFocus(int lineIndex);
    Q_INVOKABLE void clearFocus();
    // Renders at widthPx (1080 or 2160) and saves a PNG under ~/Pictures/Love Lyrics/.
    Q_INVOKABLE void exportPng(int widthPx);
    Q_INVOKABLE void copyToClipboard(int widthPx);

    // Builds the renderer input from the current state (public for tests).
    Cards::CardState cardState() const;
    Cards::CardAssets cardAssets() const { return m_assets; }

    QImage previewImage(const QString &id) const;
    static QString exportDirectory();

signals:
    void stateChanged();
    void linesChanged();
    void analysisChanged();
    void previewChanged();
    void busyChanged();
    // ok: path (export) or "" (clipboard); failure: message.
    void exportFinished(bool ok, const QString &pathOrError);

private:
    void fillFromPlayer();
    void scheduleRender();
    void renderPreview();
    void updateCover();
    void loadPreferences();
    void savePreference(const QString &key);
    void setBusy(bool busy);
    void runExport(int widthPx, bool clipboard);

    Settings *m_settings;
    MprisClient *m_player;
    PlaybackClock *m_clock;
    LyricsModel *m_lyrics;
    LyricsCursor *m_cursor;
    CoverLoader *m_cover;

    QVariantMap m_state;
    QList<int> m_selected; // line indices, sorted
    QList<int> m_focus;    // line indices
    Cards::CardAssets m_assets;

    QTimer m_renderTimer;
    quint64 m_renderRequest = 0;
    int m_previewSerial = 0;
    QUrl m_previewSource;
    double m_previewAspect = 16.0 / 9.0;
    QVariantList m_hits;
    int m_dropped = 0;
    bool m_busy = false;
    bool m_active = false;

    mutable QMutex m_mutex;
    QHash<QString, QImage> m_previews; // current + previous
};

class CardImageProvider : public QQuickImageProvider
{
public:
    explicit CardImageProvider(const CardController *controller);
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    const CardController *m_controller;
};
