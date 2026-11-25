// SPDX-License-Identifier: AGPL-3.0-or-later
#include "CardController.h"

#include "app/Settings.h"
#include "lyrics/LyricsCursor.h"
#include "lyrics/LyricsModel.h"
#include "media/CoverLoader.h"
#include "mpris/MprisClient.h"
#include "playback/PlaybackClock.h"

#include <QClipboard>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QtConcurrent>

Q_LOGGING_CATEGORY(lcCards, "lovelyrics.cards")

using namespace Cards;

namespace {

const char *const SettingsGroup = "Card";
// State keys saved between sessions (texts and selections are per song, so not saved).
const QStringList PersistentKeys = {
    QStringLiteral("template"), QStringLiteral("ratio"), QStringLiteral("style"), QStringLiteral("mode"),
    QStringLiteral("blur"), QStringLiteral("font"), QStringLiteral("scale"), QStringLiteral("align"),
    QStringLiteral("showTranslation"), QStringLiteral("dateOn"), QStringLiteral("progressOn"),
    QStringLiteral("noteOn"), QStringLiteral("noteText"), QStringLiteral("exportWidth"),
};

QVariantMap defaults()
{
    return {
        {"template", "compact"}, {"ratio", "auto"}, {"style", "content"}, {"mode", "auto"}, {"blur", "mid"},
        {"depthX", -1.0}, {"font", "sans"}, {"scale", 1.0}, {"align", "left"}, {"showTranslation", true},
        {"title", ""}, {"artist", ""}, {"seedIndex", -1}, {"dateOn", false}, {"dateText", ""},
        {"noteOn", false}, {"noteText", ""}, {"progressOn", false}, {"exportWidth", 1080},
    };
}

Template templateOf(const QString &id)
{
    if (id == u"classic")
        return Template::Classic;
    if (id == u"immersive")
        return Template::Immersive;
    if (id == u"depth")
        return Template::Depth;
    if (id == u"quiet")
        return Template::Quiet;
    return Template::Compact;
}

Ratio ratioOf(const QString &id)
{
    if (id == u"4:5")
        return Ratio::R4x5;
    if (id == u"9:16")
        return Ratio::R9x16;
    if (id == u"1:1")
        return Ratio::R1x1;
    return Ratio::Auto;
}

QString ratioId(Ratio r)
{
    switch (r) {
    case Ratio::R4x5: return QStringLiteral("4:5");
    case Ratio::R9x16: return QStringLiteral("9:16");
    case Ratio::R1x1: return QStringLiteral("1:1");
    default: return QStringLiteral("auto");
    }
}

QString formatTime(qint64 ms)
{
    const qint64 s = std::max<qint64>(0, ms) / 1000;
    return QStringLiteral("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
}

QString sanitizeFileName(QString name)
{
    static const QRegularExpression bad(QStringLiteral(R"([/\\:*?"<>|\x00-\x1f])"));
    name.replace(bad, QStringLiteral("_"));
    name = name.simplified();
    if (name.size() > 120)
        name.truncate(120);
    return name.isEmpty() ? QStringLiteral("Lyrics") : name;
}

} // namespace

CardController::CardController(Settings *settings, MprisClient *player, PlaybackClock *clock, LyricsModel *lyrics,
                               LyricsCursor *cursor, CoverLoader *cover, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_player(player)
    , m_clock(clock)
    , m_lyrics(lyrics)
    , m_cursor(cursor)
    , m_cover(cover)
    , m_state(defaults())
{
    loadPreferences();
    m_renderTimer.setSingleShot(true);
    m_renderTimer.setInterval(40);
    connect(&m_renderTimer, &QTimer::timeout, this, &CardController::renderPreview);
    connect(m_cover, &CoverLoader::changed, this, [this] {
        updateCover();
        scheduleRender();
    });
    connect(m_lyrics, &LyricsModel::documentChanged, this, [this] {
        // A new song: the old selection means nothing any more.
        if (m_active) {
            fillFromPlayer();
            return;
        }
        m_selected.clear();
        m_focus.clear();
        emit linesChanged();
    });
}

void CardController::loadPreferences()
{
    const QVariantMap d = defaults();
    for (const QString &key : PersistentKeys) {
        QVariant v = m_settings->styleValue(QString::fromLatin1(SettingsGroup), key, d.value(key));
        // Values read back from the INI file are strings: restore the default's type.
        if (d.value(key).metaType() == QMetaType::fromType<bool>())
            v = v.toString() == u"true" || v.toBool();
        else if (d.value(key).metaType() == QMetaType::fromType<double>())
            v = v.toDouble();
        else if (d.value(key).metaType() == QMetaType::fromType<int>())
            v = v.toInt();
        m_state.insert(key, v);
    }
}

void CardController::savePreference(const QString &key)
{
    if (PersistentKeys.contains(key))
        m_settings->setStyleValue(QString::fromLatin1(SettingsGroup), key, m_state.value(key));
}

QVariantList CardController::lines() const
{
    QVariantList out;
    const auto &doc = m_lyrics->document();
    for (int i = 0; i < doc.lines.size(); ++i) {
        const auto &l = doc.lines[i];
        if (l.isInstrumental || l.text.trimmed().isEmpty())
            continue;
        out.append(QVariantMap{{"index", i},
                               {"text", l.text},
                               {"translation", l.translation},
                               {"selected", m_selected.contains(i)},
                               {"focused", m_focus.contains(i)}});
    }
    return out;
}

QVariantList CardController::swatches() const
{
    QVariantList out;
    for (const auto &s : m_assets.analysis.swatches)
        out.append(QVariantMap{{"color", s.color}, {"role", s.role}});
    return out;
}

QVariantList CardController::templates() const
{
    QVariantList out;
    for (Template t : {Template::Compact, Template::Classic, Template::Immersive, Template::Depth, Template::Quiet}) {
        const TemplateInfo &i = info(t);
        out.append(QVariantMap{{"id", QString::fromLatin1(i.id)}, {"label", QString::fromLatin1(i.label)}});
    }
    return out;
}

QVariantMap CardController::templateInfo() const
{
    const TemplateInfo &i = info(templateOf(m_state.value("template").toString()));
    QStringList ratios;
    for (Ratio r : i.ratios)
        ratios << ratioId(r);
    return {{"ratios", ratios},
            {"style", i.style},
            {"blur", i.blur},
            {"crop", i.crop},
            {"progress", i.progress},
            {"note", i.note},
            {"effectiveRatio", ratioId(ratioFor(templateOf(m_state.value("template").toString()),
                                                ratioOf(m_state.value("ratio").toString())))}};
}

void CardController::open()
{
    m_active = true;
    fillFromPlayer();
}

void CardController::close()
{
    m_active = false;
}

void CardController::fillFromPlayer()
{
    m_state.insert("title", m_player->title());
    m_state.insert("artist", m_player->artist());
    m_state.insert("seedIndex", -1);
    m_state.insert("depthX", -1.0);
    m_state.insert("dateText", QDate::currentDate().toString(QStringLiteral("yyyy.MM.dd")));

    // The current line (or the first) and the next ones, up to four sung lines.
    m_selected.clear();
    m_focus.clear();
    const auto &doc = m_lyrics->document();
    // From the clock rather than the cursor: right after a track change the cursor may not
    // have caught up with the new lyrics yet.
    int start = std::max(0, LyricsCursor::lastStartedIndex(doc.lines, m_clock->lyricsPositionMs()));
    for (int i = start; i < doc.lines.size() && m_selected.size() < 4; ++i)
        if (!doc.lines[i].isInstrumental && !doc.lines[i].text.trimmed().isEmpty())
            m_selected << i;
    updateCover();
    emit stateChanged();
    emit linesChanged();
    renderPreview();
}

void CardController::set(const QString &key, const QVariant &value)
{
    if (!m_state.contains(key) || m_state.value(key) == value)
        return;
    m_state.insert(key, value);
    savePreference(key);
    emit stateChanged();
    scheduleRender();
}

void CardController::toggleLine(int lineIndex)
{
    if (m_selected.contains(lineIndex)) {
        m_selected.removeAll(lineIndex);
        m_focus.removeAll(lineIndex);
    } else {
        m_selected << lineIndex;
        std::sort(m_selected.begin(), m_selected.end());
    }
    emit linesChanged();
    scheduleRender();
}

void CardController::toggleFocus(int lineIndex)
{
    if (m_focus.contains(lineIndex))
        m_focus.removeAll(lineIndex);
    else {
        m_focus << lineIndex;
        if (!m_selected.contains(lineIndex)) {
            m_selected << lineIndex;
            std::sort(m_selected.begin(), m_selected.end());
        }
    }
    emit linesChanged();
    scheduleRender();
}

void CardController::clearFocus()
{
    if (m_focus.isEmpty())
        return;
    m_focus.clear();
    emit linesChanged();
    scheduleRender();
}

void CardController::updateCover()
{
    QImage img = m_cover->currentImage();
    if (img.cacheKey() == m_assets.cover.cacheKey() && !m_assets.cover.isNull())
        return;
    m_assets.cover = img;
    m_assets.analysis = CardColor::analyzeCover(img.isNull() ? defaultCover() : img);
    emit analysisChanged();
}

CardState CardController::cardState() const
{
    CardState st;
    const QVariantMap &s = m_state;
    st.tmpl = templateOf(s.value("template").toString());
    st.title = s.value("title").toString();
    st.artist = s.value("artist").toString();
    const auto &doc = m_lyrics->document();
    for (int i : m_selected) {
        if (i < 0 || i >= doc.lines.size())
            continue;
        if (m_focus.contains(i))
            st.focus.insert(int(st.groups.size()));
        st.groups.append({doc.lines[i].text, doc.lines[i].translation});
    }
    const int seedIndex = s.value("seedIndex").toInt();
    if (seedIndex >= 0 && seedIndex < m_assets.analysis.swatches.size())
        st.seed = m_assets.analysis.swatches[seedIndex].color;
    const QString style = s.value("style").toString();
    st.style = style == u"tonal" ? CardColor::Style::Tonal
               : style == u"vibrant" ? CardColor::Style::Vibrant
               : style == u"mono" ? CardColor::Style::Mono
                                   : CardColor::Style::Content;
    const QString mode = s.value("mode").toString();
    st.mode = mode == u"light" ? CardColor::Mode::Light : mode == u"dark" ? CardColor::Mode::Dark : CardColor::Mode::Auto;
    const QString blur = s.value("blur").toString();
    st.blur = blur == u"light" ? BlurLevel::Light : blur == u"heavy" ? BlurLevel::Heavy : BlurLevel::Mid;
    const double depthX = s.value("depthX").toDouble();
    if (depthX >= 0)
        st.depthX = std::clamp(depthX, 0.0, 1.0);
    const QString font = s.value("font").toString();
    st.font = font == u"serif" ? FontPreset::Serif : font == u"app" ? FontPreset::App : FontPreset::Sans;
    st.appFont = m_settings->fontFamily();
    st.scale = s.value("scale").toDouble();
    st.alignCenter = s.value("align").toString() == u"center";
    st.showTranslation = s.value("showTranslation").toBool();
    st.ratio = ratioOf(s.value("ratio").toString());
    st.extras.date = s.value("dateOn").toBool();
    st.extras.dateText = s.value("dateText").toString();
    st.extras.note = s.value("noteOn").toBool();
    st.extras.noteText = s.value("noteText").toString();
    st.extras.progress = s.value("progressOn").toBool();
    if (!m_selected.isEmpty() && m_selected.first() < doc.lines.size())
        st.extras.cur = formatTime(doc.lines[m_selected.first()].startMs);
    else
        st.extras.cur = formatTime(qint64(m_clock->positionMs()));
    st.extras.dur = formatTime(m_clock->durationMs());
    return st;
}

void CardController::scheduleRender()
{
    m_renderTimer.start();
}

void CardController::renderPreview()
{
    const quint64 id = ++m_renderRequest;
    const CardState st = cardState();
    const CardAssets assets = m_assets;
    // 1.5 design px per screen px keeps the preview sharp on HiDPI screens without the
    // cost of a full export.
    auto *watcher = new QFutureWatcher<CardResult>(this);
    connect(watcher, &QFutureWatcher<CardResult>::finished, this, [this, watcher, id] {
        const CardResult r = watcher->result();
        watcher->deleteLater();
        if (id != m_renderRequest)
            return;
        const int serial = ++m_previewSerial;
        {
            QMutexLocker lock(&m_mutex);
            const QString keep = QString::number(serial - 1);
            for (auto it = m_previews.begin(); it != m_previews.end();)
                it = it.key() == keep ? std::next(it) : m_previews.erase(it);
            m_previews.insert(QString::number(serial), r.image);
        }
        m_previewSource = QUrl(QStringLiteral("image://card/%1").arg(serial));
        m_previewAspect = r.height / DesignWidth;
        m_dropped = r.dropped;
        m_hits.clear();
        const int selectedCount = int(m_selected.size());
        for (const Hit &h : r.hits)
            if (h.index >= 0 && h.index < selectedCount)
                m_hits.append(QVariantMap{{"line", m_selected[h.index]},
                                          {"x", h.rect.x() / DesignWidth},
                                          {"y", h.rect.y() / r.height},
                                          {"w", h.rect.width() / DesignWidth},
                                          {"h", h.rect.height() / r.height}});
        emit previewChanged();
    });
    watcher->setFuture(QtConcurrent::run([st, assets] { return render(st, assets, 1.5); }));
}

QImage CardController::previewImage(const QString &id) const
{
    QMutexLocker lock(&m_mutex);
    return m_previews.value(id);
}

QString CardController::exportDirectory()
{
    QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (pictures.isEmpty())
        pictures = QDir::homePath();
    return pictures + QStringLiteral("/Love Lyrics");
}

void CardController::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void CardController::exportPng(int widthPx)
{
    runExport(widthPx, false);
}

void CardController::copyToClipboard(int widthPx)
{
    runExport(widthPx, true);
}

void CardController::runExport(int widthPx, bool clipboard)
{
    if (m_busy)
        return;
    setBusy(true);
    const CardState st = cardState();
    const CardAssets assets = m_assets;
    const double scale = std::clamp(widthPx, 540, 4320) / DesignWidth;
    QString path;
    if (!clipboard) {
        const QString dir = exportDirectory();
        QDir().mkpath(dir);
        QString base = st.title.trimmed();
        if (!st.artist.trimmed().isEmpty())
            base = st.artist.trimmed() + QStringLiteral(" - ") + base;
        base = sanitizeFileName(base);
        path = dir + QLatin1Char('/') + base + QStringLiteral(".png");
        for (int n = 2; QFile::exists(path); ++n)
            path = dir + QLatin1Char('/') + base + QStringLiteral(" (%1).png").arg(n);
    }

    auto *watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, path, clipboard] {
        const QImage img = watcher->result();
        watcher->deleteLater();
        setBusy(false);
        if (img.isNull()) {
            emit exportFinished(false, tr("Could not render the card"));
            return;
        }
        if (clipboard) {
            QGuiApplication::clipboard()->setImage(img);
            emit exportFinished(true, QString());
            return;
        }
        if (!img.save(path, "PNG")) {
            emit exportFinished(false, tr("Could not save %1").arg(path));
            return;
        }
        qCInfo(lcCards) << "Saved card" << path << img.size();
        emit exportFinished(true, path);
    });
    watcher->setFuture(QtConcurrent::run([st, assets, scale] {
        QImage img = render(st, assets, scale).image;
        // Exports are plain sRGB PNGs; the 72 dpi used for layout is meaningless outside.
        img.setDotsPerMeterX(3780);
        img.setDotsPerMeterY(3780);
        return img.convertToFormat(QImage::Format_ARGB32);
    }));
}

CardImageProvider::CardImageProvider(const CardController *controller)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_controller(controller)
{
}

QImage CardImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QImage img = m_controller->previewImage(id);
    if (size)
        *size = img.size();
    if (!img.isNull() && requestedSize.width() > 0 && requestedSize.height() > 0
        && (requestedSize.width() < img.width() || requestedSize.height() < img.height()))
        img = img.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return img;
}
