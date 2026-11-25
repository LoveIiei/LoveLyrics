// SPDX-License-Identifier: AGPL-3.0-or-later
// Ported from spacedge/lyric-card-generator (AGPL-3.0): src/render/index.js (fitting a
// fixed ratio), src/render/templates.js (the five templates) and src/render/common.js
// (drawing helpers). Numbers are the reference's design-board values on a 540-wide card.
// Not ported: QR codes and furigana.
#include "CardRenderer.h"

#include "TextWrap.h"

#include <QFontMetricsF>
#include <QLinearGradient>
#include <QMutex>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <list>
#include <memory>

namespace Cards {

using namespace CardColor;

namespace {

constexpr double W = DesignWidth;
constexpr int DotsPerMeter72 = 2835; // 72 dpi: 1 pt == 1 design px

const QString PlaceholderText = QStringLiteral("Lyrics will appear here");

// ---------- fonts ----------------------------------------------------------------------

struct FontSet
{
    QStringList jp, zh;
    int regular, medium, bold;
};

FontSet fontSet(const CardState &st)
{
    switch (st.font) {
    case FontPreset::Serif:
        return {{QStringLiteral("Noto Serif"), QStringLiteral("Noto Serif CJK JP"), QStringLiteral("Noto Sans CJK JP")},
                {QStringLiteral("Noto Serif"), QStringLiteral("Noto Serif CJK SC"), QStringLiteral("Noto Sans CJK SC")},
                400, 600, 600};
    case FontPreset::App:
        return {{st.appFont, QStringLiteral("Noto Sans CJK JP")}, {st.appFont, QStringLiteral("Noto Sans CJK SC")},
                400, 500, 700};
    case FontPreset::Sans:
    default:
        return {{QStringLiteral("Noto Sans"), QStringLiteral("Noto Sans CJK JP")},
                {QStringLiteral("Noto Sans"), QStringLiteral("Noto Sans CJK SC")}, 400, 500, 700};
    }
}

// Japanese stack for text with kana or plain Latin, Chinese stack for Han / Hangul-only text.
const QStringList &familyFor(const FontSet &fs, const QString &text)
{
    if (TextWrap::hasKana(text))
        return fs.jp;
    if (TextWrap::hasHan(text) || TextWrap::hasHangul(text))
        return fs.zh;
    return fs.jp;
}

QFont makeFont(int weight, double size, const QStringList &families)
{
    QFont f;
    f.setFamilies(families);
    f.setWeight(QFont::Weight(weight));
    f.setPointSizeF(std::max(1.0, size));
    f.setHintingPreference(QFont::PreferNoHinting);
    f.setStyleStrategy(QFont::PreferAntialias);
    return f;
}

QFont monoFont(double size)
{
    return makeFont(400, size, {QStringLiteral("Noto Sans Mono"), QStringLiteral("DejaVu Sans Mono"), QStringLiteral("monospace")});
}

QImage makeImage(int w, int h)
{
    QImage img(std::max(1, w), std::max(1, h), QImage::Format_ARGB32_Premultiplied);
    img.setDotsPerMeterX(DotsPerMeter72);
    img.setDotsPerMeterY(DotsPerMeter72);
    img.fill(Qt::transparent);
    return img;
}

// A painter that only measures (no image of the card yet).
struct Measurer
{
    QImage dev = makeImage(4, 4);
};

// ---------- drawing context ------------------------------------------------------------

enum class Align { Left, Center, Right };

struct Ctx
{
    QPainter *p = nullptr; // null while only measuring
    QPaintDevice *dev = nullptr;
    QFont font;
    double k = 1;

    void setFont(int weight, double size, const QStringList &families)
    {
        font = makeFont(weight, size, families);
        if (p)
            p->setFont(font);
    }
    void setFont(const QFont &f)
    {
        font = f;
        if (p)
            p->setFont(font);
    }
    double measure(const QString &s) const { return QFontMetricsF(font, dev).horizontalAdvance(s); }
    QStringList wrap(const QString &text, double maxW) const
    {
        return TextWrap::wrap([this](const QString &s) { return measure(s); }, text, maxW);
    }
    // Canvas-style fillText with textBaseline = middle.
    void text(const QString &s, double x, double yMid, Align align, const QColor &color)
    {
        if (!p)
            return;
        const QFontMetricsF fm(font, dev);
        const double w = fm.horizontalAdvance(s);
        const double x0 = align == Align::Center ? x - w / 2 : align == Align::Right ? x - w : x;
        const double baseline = yMid + (fm.ascent() - fm.descent()) / 2;
        p->setPen(color);
        p->drawText(QPointF(x0, baseline), s);
    }
};

double drawLines(Ctx &c, const QStringList &lines, double x, double y, double lh, Align align, const QColor &color,
                 bool draw)
{
    if (draw)
        for (int i = 0; i < lines.size(); ++i)
            c.text(lines[i], x, y + i * lh + lh / 2, align, color);
    return lines.size() * lh;
}

void drawSpaced(Ctx &c, const QString &text, double x, double yMid, double spacing, Align align, const QColor &color)
{
    double width = 0;
    const auto chars = text.toUcs4();
    for (char32_t u : chars)
        width += c.measure(QString::fromUcs4(&u, 1));
    width += spacing * std::max<qsizetype>(0, chars.size() - 1);
    double cx = align == Align::Right ? x - width : x;
    for (char32_t u : chars) {
        const QString ch = QString::fromUcs4(&u, 1);
        c.text(ch, cx, yMid, Align::Left, color);
        cx += c.measure(ch) + spacing;
    }
}

QString ellipsize(const Ctx &c, const QString &text, double maxW)
{
    if (c.measure(text) <= maxW)
        return text;
    auto chars = text.toUcs4();
    auto str = [&chars] { return QString::fromUcs4(reinterpret_cast<const char32_t *>(chars.constData()), chars.size()); };
    while (!chars.isEmpty() && c.measure(str() + QStringLiteral("…")) > maxW)
        chars.removeLast();
    return str() + QStringLiteral("…");
}

QPainterPath roundedRect(double x, double y, double w, double h, double r)
{
    QPainterPath path;
    const double rad = std::max(0.0, std::min({r, w / 2, h / 2}));
    path.addRoundedRect(QRectF(x, y, w, h), rad, rad);
    return path;
}

// object-fit: cover; fy picks which part survives a vertical crop (0 = top, 0.5 = centre).
void drawCoverFit(QPainter *p, const QImage &src, double dx, double dy, double dw, double dh, double fy = 0.5)
{
    const double sw = src.width(), sh = src.height();
    if (sw <= 0 || sh <= 0)
        return;
    const double s = std::max(dw / sw, dh / sh);
    const double cw = dw / s, ch = dh / s;
    p->drawImage(QRectF(dx, dy, dw, dh), src, QRectF((sw - cw) / 2, (sh - ch) * fy, cw, ch));
}

// ---------- blur -----------------------------------------------------------------------

void boxBlurH(QImage &img, int radius)
{
    if (radius < 1)
        return;
    const int w = img.width(), h = img.height(), window = radius * 2 + 1;
    std::vector<QRgb> row(static_cast<size_t>(w));
    for (int y = 0; y < h; ++y) {
        auto *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        std::copy(line, line + w, row.begin());
        int sr = 0, sg = 0, sb = 0, sa = 0;
        auto px = [&](int x) { return row[size_t(std::clamp(x, 0, w - 1))]; };
        for (int x = -radius; x <= radius; ++x) {
            const QRgb q = px(x);
            sr += qRed(q), sg += qGreen(q), sb += qBlue(q), sa += qAlpha(q);
        }
        for (int x = 0; x < w; ++x) {
            line[x] = qRgba(sr / window, sg / window, sb / window, sa / window);
            const QRgb out = px(x - radius), in = px(x + radius + 1);
            sr += qRed(in) - qRed(out), sg += qGreen(in) - qGreen(out);
            sb += qBlue(in) - qBlue(out), sa += qAlpha(in) - qAlpha(out);
        }
    }
}

QImage transposed(const QImage &img)
{
    return img.transformed(QTransform(0, 1, 1, 0, 0, 0));
}

struct BlurKey
{
    qint64 image;
    int w, h, sigma10;
    bool operator==(const BlurKey &) const = default;
};
QMutex blurMutex;
std::list<std::pair<BlurKey, QImage>> blurCache; // small LRU, previews only

QImage blurNow(const QImage &src, int width, int height, double sigma)
{
    // Work at a reduced size so the box radius stays small; upscaling a blurred image is
    // indistinguishable from blurring at full size.
    const double factor = std::max(1.0, sigma / 4.0);
    const int sw = std::max(4, int(std::lround(width / factor)));
    const int sh = std::max(4, int(std::lround(height / factor)));
    const double s = sigma / (double(width) / sw);
    QImage img(sw, sh, QImage::Format_ARGB32_Premultiplied);
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        drawCoverFit(&p, src, 0, 0, sw, sh);
    }
    // Three box passes of this radius approximate a Gaussian of sigma s.
    const int radius = std::max(0, int(std::lround((std::sqrt(4 * s * s + 1) - 1) / 2)));
    for (int pass = 0; pass < 3; ++pass)
        boxBlurH(img, radius);
    img = transposed(img);
    for (int pass = 0; pass < 3; ++pass)
        boxBlurH(img, radius);
    img = transposed(img);
    return img.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QImage blurCached(const QImage &src, int width, int height, double sigma, bool cache)
{
    if (!cache)
        return blurNow(src, width, height, sigma);
    const BlurKey key{src.cacheKey(), width, height, int(std::lround(sigma * 10))};
    {
        QMutexLocker lock(&blurMutex);
        for (auto it = blurCache.begin(); it != blurCache.end(); ++it)
            if (it->first == key) {
                blurCache.splice(blurCache.begin(), blurCache, it);
                return it->second;
            }
    }
    QImage out = blurNow(src, width, height, sigma);
    QMutexLocker lock(&blurMutex);
    blurCache.emplace_front(key, out);
    while (blurCache.size() > 6)
        blurCache.pop_back();
    return out;
}

// Blurred rounded-rect shadow (canvas shadowBlur ≈ 2 sigma).
void drawShadow(Ctx &c, double x, double y, double w, double h, double radius, const QColor &color, double blur,
                double offY)
{
    if (!c.p)
        return;
    const double sigma = blur / 2 * c.k;
    const int pad = int(std::ceil(sigma * 3));
    const int iw = int(std::ceil(w * c.k)) + pad * 2, ih = int(std::ceil(h * c.k)) + pad * 2;
    QImage img(iw, ih, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter q(&img);
        q.setRenderHint(QPainter::Antialiasing);
        q.fillPath(roundedRect(pad, pad, w * c.k, h * c.k, radius * c.k), color);
    }
    const int r = std::max(0, int(std::lround((std::sqrt(4 * sigma * sigma + 1) - 1) / 2)));
    for (int pass = 0; pass < 3; ++pass)
        boxBlurH(img, r);
    img = transposed(img);
    for (int pass = 0; pass < 3; ++pass)
        boxBlurH(img, r);
    img = transposed(img);
    c.p->drawImage(QRectF(x - pad / c.k, y + offY - pad / c.k, iw / c.k, ih / c.k), img);
}

void drawCoverCard(Ctx &c, const QImage &src, double x, double y, double size, double radius, const QColor *shadow,
                   double shadowBlur, double shadowY, double h = -1)
{
    if (!c.p)
        return;
    if (h < 0)
        h = size;
    if (shadow)
        drawShadow(c, x, y, size, h, radius, *shadow, shadowBlur, shadowY);
    c.p->save();
    c.p->setClipPath(roundedRect(x, y, size, h, radius));
    drawCoverFit(c.p, src, x, y, size, h);
    c.p->restore();
}

// MD3 wavy progress: wave up to the playhead, flat track after it, a dot at the end.
void drawWave(QPainter *p, double x, double y, double w, double frac, const QColor &active, const QColor &track,
              double stroke = 5, double amp = 3.5, double wl = 30)
{
    const double end = x + w * frac;
    QPen pen(active, stroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    const double x0 = x + stroke / 2;
    const double waveEnd = std::max(x0, end - stroke);
    if (waveEnd > x0) {
        QPainterPath path(QPointF(x0, y));
        for (double px = x0; px <= waveEnd; px += 0.75)
            path.lineTo(px, y - std::sin((px - x) / wl * M_PI * 2) * amp);
        p->strokePath(path, pen);
    }
    const double ts = std::max(x0, end + stroke * 1.6), te = x + w - stroke * 2.4;
    if (te > ts) {
        pen.setColor(track);
        p->setPen(pen);
        p->drawLine(QPointF(ts, y), QPointF(te, y));
    }
    p->setPen(Qt::NoPen);
    p->setBrush(active);
    p->drawEllipse(QPointF(x + w - stroke / 2, y), stroke / 2, stroke / 2);
}

double parseTime(const QString &s)
{
    static const QRegularExpression re(QStringLiteral(R"(^(\d{1,3})(?::(\d{1,2}))?$)"));
    const auto m = re.match(s.trimmed());
    if (!m.hasMatch())
        return std::nan("");
    return m.capturedView(2).isEmpty() ? m.captured(1).toDouble()
                                       : m.captured(1).toDouble() * 60 + m.captured(2).toDouble();
}

double progressFraction(const Extras &ex)
{
    const double cur = parseTime(ex.cur), dur = parseTime(ex.dur);
    if (!(dur > 0) || !(cur >= 0))
        return 0.45;
    return std::clamp(cur / dur, 0.0, 1.0);
}

// Vertical Japanese text (留白): brackets, long-vowel marks and Latin rotate 90°; 、。 move
// to the upper right of their cell.
bool rotatesVertical(char32_t u)
{
    static const QString rot = QStringLiteral("ー〜～…‥—―-－–()（）「」『』【】〈〉《》[]［］{}｛｝<>＜＞=＝~");
    return u < 0x10000 && rot.contains(QChar(char16_t(u)));
}
bool cornerVertical(char32_t u)
{
    static const QString corner = QStringLiteral("、。，．,.");
    return u < 0x10000 && corner.contains(QChar(char16_t(u)));
}
bool isLatinDigit(char32_t u)
{
    return (u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') || (u >= '0' && u <= '9');
}
double verticalHeight(const Ctx &c, const QString &text, double size, double spacingEm)
{
    double h = 0;
    for (char32_t u : text.toUcs4()) {
        if (u == ' ' || u == 0x3000) {
            h += size * 0.5;
            continue;
        }
        h += (isLatinDigit(u) ? c.measure(QString::fromUcs4(&u, 1)) : size) + size * spacingEm;
    }
    return std::max(0.0, h - size * spacingEm);
}
void drawVertical(Ctx &c, const QString &text, double cx, double y, double size, double spacingEm, const QColor &color)
{
    if (!c.p)
        return;
    double yy = y;
    for (char32_t u : text.toUcs4()) {
        if (u == ' ' || u == 0x3000) {
            yy += size * 0.5;
            continue;
        }
        const QString ch = QString::fromUcs4(&u, 1);
        const bool latin = isLatinDigit(u);
        const double adv = latin ? c.measure(ch) : size;
        c.p->save();
        c.p->translate(cx, yy + adv / 2);
        if (latin || rotatesVertical(u))
            c.p->rotate(90);
        if (cornerVertical(u))
            c.p->translate(size * 0.55, -size * 0.55);
        c.text(ch, 0, 0, Align::Center, color);
        c.p->restore();
        yy += adv + size * spacingEm;
    }
}

// ---------- layout environment ---------------------------------------------------------

struct Spec
{
    double oSize, oLh;
    int oW;
    double tSize, tLh;
    int tW;
    double gap;
};

struct Measured
{
    int idx;
    QStringList ol, tl;
    QStringList famO, famT;
    double h;
    Spec spec;
};

struct GroupRef
{
    int idx;
    Group g;
};

struct Env
{
    const CardState &st;
    const CardAssets &assets;
    QImage cover;
    FontSet fs;
    CardPalette pal;
    bool darkMode;
    QList<GroupRef> groups;
    bool hasFocus;
    double H; // 0 = auto height
    double canvasH = 0;
    bool forExport = false;
    QList<Hit> hits;
    const TemplateInfo *info;
};

struct Params
{
    double fs;
    double cover;
    QList<GroupRef> groups;
    double off = 0;
};

Measured measureGroup(Ctx &c, const Env &env, const GroupRef &g, double maxW, const Spec &spec)
{
    Measured m{g.idx, {}, {}, familyFor(env.fs, g.g.original), {}, 0, spec};
    c.setFont(spec.oW, spec.oSize, m.famO);
    m.ol = c.wrap(g.g.original, maxW);
    if (env.st.showTranslation && !g.g.translation.trimmed().isEmpty()) {
        m.famT = TextWrap::hasKana(g.g.translation) ? env.fs.jp : env.fs.zh;
        c.setFont(spec.tW, spec.tSize, m.famT);
        m.tl = c.wrap(g.g.translation, maxW);
    }
    m.h = m.ol.size() * spec.oLh + (m.tl.isEmpty() ? 0 : spec.gap + m.tl.size() * spec.tLh);
    return m;
}

void drawGroup(Ctx &c, const Measured &m, double x, double y, const QColor &colO, const QColor &colT, Align align)
{
    c.setFont(m.spec.oW, m.spec.oSize, m.famO);
    const double oh = drawLines(c, m.ol, x, y, m.spec.oLh, align, colO, true);
    if (!m.tl.isEmpty()) {
        c.setFont(m.spec.tW, m.spec.tSize, m.famT);
        drawLines(c, m.tl, x, y + oh + m.spec.gap, m.spec.tLh, align, colT, true);
    }
}

double stackHeight(const QList<Measured> &ms, double gap)
{
    double h = 0;
    for (int i = 0; i < ms.size(); ++i)
        h += ms[i].h + (i ? gap : 0);
    return h;
}

void pushHit(Env &env, int idx, double x, double y, double w, double h, double gap)
{
    env.hits.append({idx, QRectF(x, y - gap / 2, w, h + gap)});
}

void placeholder(Ctx &c, const Env &env, double x, double y, Align align, const QColor &color, bool draw)
{
    if (!draw)
        return;
    c.setFont(env.fs.regular, 15, env.fs.zh);
    drawLines(c, {PlaceholderText}, x, y, 22, align, color, true);
}

struct TitleOpts
{
    double x, y, maxW;
    Align align;
    bool draw;
    double tSize, tLh;
    int tW;
    QColor tColor;
    double aSize, aLh;
    int aW;
    QColor aColor;
    double gap;
    int maxLines = 3;
};

double titleBlock(Ctx &c, const Env &env, const TitleOpts &o)
{
    const QString t = env.st.title.trimmed(), a = env.st.artist.trimmed();
    double y = o.y;
    if (!t.isEmpty()) {
        c.setFont(o.tW, o.tSize, familyFor(env.fs, t));
        const QStringList ls = c.wrap(t, o.maxW).mid(0, o.maxLines);
        y += drawLines(c, ls, o.x, y, o.tLh, o.align, o.tColor, o.draw);
    }
    if (!a.isEmpty()) {
        if (!t.isEmpty())
            y += o.gap;
        c.setFont(o.aW, o.aSize, familyFor(env.fs, a));
        const QStringList ls = c.wrap(a, o.maxW).mid(0, o.maxLines);
        y += drawLines(c, ls, o.x, y, o.aLh, o.align, o.aColor, o.draw);
    }
    return y;
}

void timesRow(Ctx &c, const Env &env, double x, double right, double y, const QColor &color)
{
    c.setFont(monoFont(12));
    c.text(env.st.extras.cur, x, y, Align::Left, color);
    c.text(env.st.extras.dur, right, y, Align::Right, color);
}

// Optional date stamp at the bottom of the long templates (the reference's QR code is not ported).
double footer(Ctx &c, const Env &env, double x, double right, double y, double gapTop, const QColor &dateColor,
              bool draw)
{
    Q_UNUSED(x);
    const auto &ex = env.st.extras;
    if (!(ex.date && !ex.dateText.trimmed().isEmpty()))
        return y;
    y += gapTop;
    const double rowH = 16;
    if (draw) {
        c.setFont(monoFont(11));
        drawSpaced(c, ex.dateText.trimmed(), right, y + rowH / 2, 0.6, Align::Right, dateColor);
    }
    return y + rowH;
}

Align alignOf(const CardState &st)
{
    return st.alignCenter ? Align::Center : Align::Left;
}

QColor colorWithAlphaOf(const QColor &c, double a)
{
    return withAlpha(c, a * c.alphaF());
}

// ---------- 紧凑 Compact ----------------------------------------------------------------

double layoutCompact(Ctx &c, Env &env, const Params &p, bool draw)
{
    const auto &st = env.st;
    const auto &ex = st.extras;
    const double fs = p.fs;
    const auto &pal = env.pal;
    const Align align = alignOf(st);
    const double off = p.off;
    const double padX = 40, innerW = 460;
    if (draw)
        c.p->fillRect(QRectF(0, 0, W, env.canvasH), pal.bg);

    double y = 48 + off;
    const double cov = 80, hp = 16, hg = 18;
    const double metaX = padX + hp + cov + hg, metaW = innerW - hp * 2 - cov - hg;
    TitleOpts meta{metaX, 0, metaW, Align::Left, false, 24 * fs, 32 * fs, env.fs.medium, pal.on,
                   16 * fs, 24 * fs, env.fs.regular, pal.onVar, 4, 2};
    const double metaH = titleBlock(c, env, meta);
    const double headH = std::max(cov, metaH) + hp * 2;
    if (draw) {
        c.p->fillPath(roundedRect(padX, y, innerW, headH, 24), pal.container);
        drawCoverCard(c, env.cover, padX + hp, y + (headH - cov) / 2, cov, 16, &pal.shadow, 20, 8);
        meta.y = y + (headH - metaH) / 2;
        meta.draw = true;
        titleBlock(c, env, meta);
    }
    y += headH + 40;

    const Spec base{23 * fs, 34 * fs, env.fs.medium, 16 * fs, 25 * fs, env.fs.regular, 6 * fs};
    Spec bold = base;
    bold.oW = env.fs.bold;
    const double lx = align == Align::Center ? W / 2 : padX + 4, maxW = innerW - 8;
    QList<Measured> ms;
    for (const auto &g : p.groups)
        ms << measureGroup(c, env, g, maxW, st.focus.contains(g.idx) ? bold : base);
    const double gap = 28 * fs;
    if (ms.isEmpty()) {
        placeholder(c, env, lx, y, align, pal.onVar, draw);
        y += 22;
    }
    for (int i = 0; i < ms.size(); ++i) {
        if (i)
            y += gap;
        const bool on = st.focus.contains(ms[i].idx);
        if (draw) {
            QColor o = pal.on, t = pal.onVar;
            if (env.hasFocus && on)
                o = pal.accent;
            else if (env.hasFocus) {
                o = colorWithAlphaOf(pal.on, 0.5);
                t = colorWithAlphaOf(pal.onVar, 0.5);
            }
            drawGroup(c, ms[i], lx, y, o, t, align);
        }
        pushHit(env, ms[i].idx, padX, y, innerW, ms[i].h, gap);
        y += ms[i].h;
    }

    if (ex.progress) {
        y += 36;
        if (draw) {
            drawWave(c.p, padX, y + 10, innerW, progressFraction(ex), pal.accent, pal.track, 4, 3, 26);
            timesRow(c, env, padX, padX + innerW, y + 32, pal.onVar);
        }
        y += 42;
    }
    y = footer(c, env, padX, padX + innerW, y, 36, pal.onVar, draw);
    return y + 56 - off;
}

// ---------- 经典 Classic ----------------------------------------------------------------

double layoutClassic(Ctx &c, Env &env, const Params &p, bool draw)
{
    const auto &st = env.st;
    const double fs = p.fs;
    const auto &pal = env.pal;
    const Align align = alignOf(st);
    const double ch = p.cover;
    // Fade the cover into the background only when its bottom edge is close to it.
    const double edgeGap = okDistance(env.assets.analysis.edgeBottom, pal.bg);
    const double fade = edgeGap <= 0.1 ? 132 : edgeGap <= 0.22 ? 56 : 0;
    if (draw) {
        c.p->fillRect(QRectF(0, 0, W, env.canvasH), pal.bg);
        drawCoverFit(c.p, env.cover, 0, 0, W, ch, 0.3);
        if (fade > 0) {
            QLinearGradient g(0, ch - fade, 0, ch);
            g.setColorAt(0, withAlpha(pal.bg, 0));
            g.setColorAt(1, pal.bg);
            c.p->fillRect(QRectF(0, ch - fade, W, fade + 1), g);
        }
    }
    const bool hasMeta = !st.title.trimmed().isEmpty() || !st.artist.trimmed().isEmpty();
    double y = !hasMeta ? ch : fade == 132 ? ch - 36 : ch + 28;
    y = titleBlock(c, env, {align == Align::Center ? W / 2 : 36, y, 468, align, draw, 28 * fs, 36 * fs, env.fs.medium,
                            pal.on, 15 * fs, 22 * fs, env.fs.regular, pal.onVar, 4});
    y += 40;

    const Spec spec{21 * fs, 31 * fs, env.fs.medium, 14.5 * fs, 22 * fs, env.fs.regular, 6 * fs};
    const double lx = align == Align::Center ? W / 2 : 36;
    QList<Measured> ms;
    for (const auto &g : p.groups)
        ms << measureGroup(c, env, g, 468, spec);
    const double gap = 32 * fs;
    if (ms.isEmpty()) {
        placeholder(c, env, lx, y, align, pal.onVar, draw);
        y += 22;
    }
    for (int i = 0; i < ms.size(); ++i) {
        if (i)
            y += gap;
        const bool on = st.focus.contains(ms[i].idx);
        if (draw)
            drawGroup(c, ms[i], lx, y, on ? pal.accent : pal.on, on ? pal.accent : pal.onVar, align);
        pushHit(env, ms[i].idx, 24, y, 492, ms[i].h, gap);
        y += ms[i].h;
    }
    y = footer(c, env, 36, 504, y, 52, pal.onVar, draw);
    return y + 64;
}

// ---------- photo templates: shared pieces ---------------------------------------------

struct Ink
{
    QColor ink, ink2, sub, time, wave, track, tone;
};

Ink photoInk(bool dark)
{
    if (dark)
        return {Qt::white, QColor(255, 255, 255, 204), QColor(255, 255, 255, 199), QColor(255, 255, 255, 184),
                Qt::white, QColor(255, 255, 255, 77), Qt::black};
    return {QColor(0x15, 0x17, 0x1C), QColor(21, 23, 28, 199), QColor(21, 23, 28, 184), QColor(21, 23, 28, 179),
            QColor(0x15, 0x17, 0x1C), QColor(21, 23, 28, 64), Qt::white};
}

// Gradients only where text sits: a band at the top for the title row, one under the lyrics.
void textShades(QPainter *p, double H, double lyricsTop, const Ink &ink, bool dark)
{
    QLinearGradient top(0, 0, 0, 120);
    top.setColorAt(0, withAlpha(ink.tone, dark ? 0.45 : 0.55));
    top.setColorAt(1, withAlpha(ink.tone, 0));
    p->fillRect(QRectF(0, 0, W, 120), top);
    const double from = std::max(0.0, lyricsTop - 90);
    QLinearGradient bot(0, from, 0, H);
    bot.setColorAt(0, withAlpha(ink.tone, 0));
    bot.setColorAt(0.3, withAlpha(ink.tone, dark ? 0.42 : 0.55));
    bot.setColorAt(1, withAlpha(ink.tone, dark ? 0.64 : 0.78));
    p->fillRect(QRectF(0, from, W, H - from), bot);
}

void photoHeader(Ctx &c, const Env &env, const Ink &ink)
{
    const auto &st = env.st;
    const auto &ex = st.extras;
    drawCoverCard(c, env.cover, 32, 32, 64, 14, nullptr, 0, 0);
    const bool showDate = ex.date && !ex.dateText.trimmed().isEmpty();
    double dateW = 0;
    if (showDate) {
        c.setFont(monoFont(11));
        dateW = c.measure(ex.dateText.trimmed()) + ex.dateText.trimmed().size() * 0.6 + 16;
    }
    const double tx = 110, tMax = W - tx - 32 - dateW;
    const QString t = st.title.trimmed(), a = st.artist.trimmed();
    if (!t.isEmpty()) {
        c.setFont(env.fs.medium, 17, familyFor(env.fs, t));
        c.text(ellipsize(c, t, tMax), tx, a.isEmpty() ? 64 : 52, Align::Left, ink.ink);
    }
    if (!a.isEmpty()) {
        c.setFont(env.fs.regular, 13, familyFor(env.fs, a));
        c.text(ellipsize(c, a, tMax), tx, t.isEmpty() ? 64 : 75, Align::Left, ink.sub);
    }
    if (showDate) {
        c.setFont(monoFont(11));
        drawSpaced(c, ex.dateText.trimmed(), W - 32, 64, 0.6, Align::Right, ink.time);
    }
}

struct PhotoLyrics
{
    double natural, top;
    QList<Measured> ms;
    double gap;
    bool showProg;
};

PhotoLyrics photoLyrics(Ctx &c, const Env &env, const Params &p, double H)
{
    const double fs = p.fs;
    const Spec spec{28 * fs, 38 * fs, env.fs.bold, 16 * fs, 24 * fs, env.fs.regular, 6 * fs};
    PhotoLyrics r;
    r.gap = 28 * fs;
    for (const auto &g : p.groups)
        r.ms << measureGroup(c, env, g, 476, spec);
    const double lyrH = r.ms.isEmpty() ? 22 : stackHeight(r.ms, r.gap);
    r.showProg = env.st.extras.progress;
    const double bottom = r.showProg ? 32 + 42 : 0;
    r.natural = 32 + 64 + 32 + lyrH + bottom + 32;
    r.top = std::max(128.0, (H > 0 ? H : r.natural) - 32 - bottom - lyrH);
    return r;
}

void drawPhotoLyrics(Ctx &c, Env &env, const PhotoLyrics &r, double H, const Ink &ink)
{
    const auto &st = env.st;
    const Align align = alignOf(st);
    const double lx = align == Align::Center ? W / 2 : 32;
    double ly = r.top;
    if (r.ms.isEmpty())
        placeholder(c, env, lx, ly, align, ink.ink2, true);
    for (int i = 0; i < r.ms.size(); ++i) {
        if (i)
            ly += r.gap;
        const bool on = st.focus.contains(r.ms[i].idx);
        c.p->setOpacity(env.hasFocus && !on ? 0.4 : 1.0);
        drawGroup(c, r.ms[i], lx, ly, ink.ink, ink.ink2, align);
        c.p->setOpacity(1.0);
        pushHit(env, r.ms[i].idx, 24, ly, 492, r.ms[i].h, r.gap);
        ly += r.ms[i].h;
    }
    if (r.showProg) {
        const double py = H - 32 - 42;
        drawWave(c.p, 32, py + 10, 476, progressFraction(st.extras), ink.wave, ink.track, 4, 3, 30);
        timesRow(c, env, 32, W - 32, py + 34, ink.time);
    }
}

int dev(const Env &env, const Ctx &c, double v)
{
    Q_UNUSED(env);
    return std::max(1, int(std::lround(v * c.k)));
}

// A card-sized layer in device px, painted with the same transform as the card.
struct Layer
{
    QImage img;
    std::unique_ptr<QPainter> p;
};

Layer cardLayer(const Ctx &c, double H)
{
    Layer l;
    l.img = makeImage(int(std::lround(W * c.k)), int(std::lround(H * c.k)));
    l.p = std::make_unique<QPainter>(&l.img);
    l.p->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
    l.p->scale(c.k, c.k);
    return l;
}

// Keep a vertical band of a layer: stops are (position 0..1, alpha).
void maskVertical(Layer &layer, double H, const QList<std::pair<double, double>> &stops)
{
    layer.p->setCompositionMode(QPainter::CompositionMode_DestinationIn);
    QLinearGradient m(0, 0, 0, H);
    for (const auto &[pos, a] : stops)
        m.setColorAt(std::clamp(pos, 0.0, 1.0), QColor(0, 0, 0, int(std::lround(a * 255))));
    layer.p->fillRect(QRectF(0, 0, W, H), m);
    layer.p->setCompositionMode(QPainter::CompositionMode_SourceOver);
    layer.p->end();
}

double blurSigma(BlurLevel b)
{
    switch (b) {
    case BlurLevel::Light: return 6;
    case BlurLevel::Heavy: return 24;
    default: return 14;
    }
}

double depthFar(BlurLevel b)
{
    switch (b) {
    case BlurLevel::Light: return 18;
    case BlurLevel::Heavy: return 44;
    default: return 30;
    }
}

// ---------- 沉浸 Immersive: whole cover, lightly blurred -------------------------------

double layoutImmersive(Ctx &c, Env &env, const Params &p, bool draw)
{
    const double H = env.H > 0 ? env.H : 960;
    const PhotoLyrics lyr = photoLyrics(c, env, p, H);
    if (!draw)
        return lyr.natural;

    const bool dark = env.darkMode;
    const Ink ink = photoInk(dark);
    const QImage &src = env.cover;
    const double sigma = blurSigma(env.st.blur);
    const bool cache = !env.forExport;
    // Far layer: heavily blurred, filling the whole frame.
    const double side = std::max(W, H);
    const QImage far = blurCached(src, dev(env, c, side), dev(env, c, side), 48 * c.k, cache);
    c.p->drawImage(QRectF((W - side) / 2, (H - side) / 2, side, side), far);
    // Near layer: the square cover, lightly blurred, fading into the far layer at top and bottom.
    const QImage near = blurCached(src, dev(env, c, W), dev(env, c, W), sigma * c.k, cache);
    const double y0 = H > W ? std::round((H - W) / 2) : 0;
    if (H > W) {
        Layer layer = cardLayer(c, H);
        layer.p->drawImage(QRectF(0, y0, W, W), near);
        const double fade = std::min(120.0, (H - W) / 2 + 60);
        maskVertical(layer, H, {{0, 0}, {y0 / H, 0}, {(y0 + fade) / H, 1}, {(y0 + W - fade) / H, 1}, {(y0 + W) / H, 0}, {1, 0}});
        c.p->drawImage(QRectF(0, 0, W, H), layer.img);
    } else {
        c.p->drawImage(QRectF(0, 0, W, W), near);
    }
    c.p->fillRect(QRectF(0, 0, W, H), withAlpha(ink.tone, dark ? 0.18 : 0.22));
    textShades(c.p, H, lyr.top, ink, dark);
    photoHeader(c, env, ink);
    drawPhotoLyrics(c, env, lyr, H, ink);
    return lyr.natural;
}

// ---------- 景深 Depth: cover fills the frame, sharp top, soft bottom -------------------

struct Crop
{
    double S, x0, y0;
};

Crop depthCrop(const CardState &st, const CoverAnalysis &a, double H)
{
    const double S = std::round(H * 1.04);
    const double fx = st.depthX.value_or(a.focus.x());
    const double x0 = std::min(0.0, std::max(W - S, std::round(W / 2 - fx * S)));
    return {S, x0, std::round((H - S) / 2)};
}

double layoutDepth(Ctx &c, Env &env, const Params &p, bool draw)
{
    const double H = env.H > 0 ? env.H : 960;
    const PhotoLyrics lyr = photoLyrics(c, env, p, H);
    if (!draw)
        return lyr.natural;

    const bool dark = env.darkMode;
    const Ink ink = photoInk(dark);
    const QImage &src = env.cover;
    const Crop crop = depthCrop(env.st, env.assets.analysis, H);
    const double farSigma = depthFar(env.st.blur);
    const int size = dev(env, c, crop.S);
    const bool cache = !env.forExport;
    const QImage far = blurCached(src, size, size, farSigma * c.k, cache);
    const QImage mid = blurCached(src, size, size, farSigma / 3 * c.k, cache);
    const QRectF where(crop.x0, crop.y0, crop.S, crop.S);
    c.p->drawImage(where, far);
    {
        Layer midLayer = cardLayer(c, H);
        midLayer.p->drawImage(where, mid);
        maskVertical(midLayer, H, {{0, 1}, {0.55, 1}, {0.78, 0}, {1, 0}});
        c.p->drawImage(QRectF(0, 0, W, H), midLayer.img);
    }
    {
        Layer nearLayer = cardLayer(c, H);
        drawCoverFit(nearLayer.p.get(), src, crop.x0, crop.y0, crop.S, crop.S);
        maskVertical(nearLayer, H, {{0, 1}, {0.32, 1}, {0.55, 0}, {1, 0}});
        c.p->drawImage(QRectF(0, 0, W, H), nearLayer.img);
    }
    c.p->fillRect(QRectF(0, 0, W, H), withAlpha(ink.tone, dark ? 0.1 : 0.14));
    textShades(c.p, H, lyr.top, ink, dark);
    photoHeader(c, env, ink);
    drawPhotoLyrics(c, env, lyr, H, ink);
    return lyr.natural;
}

// ---------- 留白 Quiet ------------------------------------------------------------------

double layoutQuiet(Ctx &c, Env &env, const Params &p, bool draw)
{
    const auto &st = env.st;
    const auto &ex = st.extras;
    const double fs = p.fs;
    const double fixedH = env.H;
    const double baseH = fixedH > 0 ? fixedH : 960;
    const Align align = alignOf(st);
    const double topY = baseH <= 600 ? 52 : baseH <= 720 ? 74 : 96;
    const double minGap = baseH <= 600 ? 32 : 52;
    const double cs = fixedH > 0 ? std::min(p.cover, std::round(fixedH * 0.3)) : p.cover;

    const Spec spec{20 * fs, 30 * fs, env.fs.medium, 13.5 * fs, 20.5 * fs, env.fs.regular, 5 * fs};
    Spec boldSpec = spec;
    boldSpec.oW = env.fs.bold;
    const double gap = 22 * fs;
    QList<Measured> ms;
    for (const auto &g : p.groups)
        ms << measureGroup(c, env, g, 460, st.focus.contains(g.idx) ? boldSpec : spec);
    const double lyrH = ms.isEmpty() ? 22 : stackHeight(ms, gap);

    const bool showProg = ex.progress;
    const bool showNote = ex.note && !ex.noteText.trimmed().isEmpty();
    const double progBlockH = showProg ? 40 + 32 : 0;
    const double bottomPad = showNote ? 76 : (baseH <= 600 ? 44 : 68);
    const double minTop = topY + cs + minGap;
    // Generous whitespace for short lyrics; rise smoothly as more lines are added.
    const double idealTop = baseH - bottomPad - progBlockH - lyrH;
    const double sparseCap = std::round(baseH * 0.63);
    const double lyTop = std::max(minTop, std::min(idealTop, sparseCap));
    const double contentEnd = lyTop + lyrH + progBlockH + bottomPad;
    const double need = fixedH > 0 ? contentEnd : std::max(960.0, contentEnd);
    if (!draw)
        return need;

    const auto &P = env.pal;
    const QColor ink = P.on, ink2 = P.onVar, dot = P.accent, track = withAlpha(P.on, 0.18);
    c.p->fillRect(QRectF(0, 0, W, env.canvasH), P.bg);

    if (ex.date && !ex.dateText.trimmed().isEmpty()) {
        c.setFont(monoFont(11));
        drawSpaced(c, ex.dateText.trimmed(), W - 40, std::max(30.0, topY / 2), 1.3, Align::Right, ink2);
    }

    // A faint shadow keeps the cover's outline when the background is its own edge colour.
    const bool blends = std::min(okDistance(env.assets.analysis.edgeTop, P.bg),
                                 okDistance(env.assets.analysis.edgeBottom, P.bg)) < 0.08;
    const QColor shadowColor = P.isDark ? QColor(0, 0, 0, 115) : withAlpha(P.on, 0.22);
    drawCoverCard(c, env.cover, 40, topY, cs, 6, blends ? &shadowColor : nullptr, 18, 6);

    // Vertical title and artist on the right edge, shrunk if they would reach the lyrics.
    const double maxV = std::max(96.0, lyTop - 36 - topY);
    const QString t = st.title.trimmed(), a = st.artist.trimmed();
    if (!t.isEmpty()) {
        const auto &fam = familyFor(env.fs, t);
        double size = 22 * fs;
        c.setFont(env.fs.medium, size, fam);
        const double h0 = verticalHeight(c, t, size, 0.32);
        if (h0 > maxV) {
            size = std::max(12.0, size * maxV / h0);
            c.setFont(env.fs.medium, size, fam);
        }
        drawVertical(c, t, W - 44 - 14, topY, size, 0.32, ink);
    }
    if (!a.isEmpty()) {
        const auto &fam = familyFor(env.fs, a);
        double size = 12 * fs;
        c.setFont(env.fs.regular, size, fam);
        const double h0 = verticalHeight(c, a, size, 0.28);
        if (h0 > maxV) {
            size = std::max(9.0, size * maxV / h0);
            c.setFont(env.fs.regular, size, fam);
        }
        drawVertical(c, a, W - 44 - (t.isEmpty() ? 8 : 28 + 14 + 8), topY, size, 0.28, ink2);
    }

    double ly = lyTop;
    const double lx = align == Align::Center ? W / 2 : 40;
    if (ms.isEmpty())
        placeholder(c, env, lx, ly, align, ink2, true);
    for (int i = 0; i < ms.size(); ++i) {
        if (i)
            ly += gap;
        const bool on = st.focus.contains(ms[i].idx);
        const bool full = !env.hasFocus || on;
        drawGroup(c, ms[i], lx, ly, full ? ink : withAlpha(ink, 0.44), full ? ink2 : withAlpha(ink2, 0.48), align);
        pushHit(env, ms[i].idx, 24, ly, 492, ms[i].h, gap);
        ly += ms[i].h;
    }

    if (showProg) {
        const double py = ly + 40;
        const double frac = progressFraction(ex);
        c.p->fillRect(QRectF(40, py + 3.25, 460, 1.5), track);
        c.p->fillRect(QRectF(40, py + 3.25, 460 * frac, 1.5), ink);
        c.p->setPen(Qt::NoPen);
        c.p->setBrush(dot);
        c.p->drawEllipse(QPointF(40 + 460 * frac, py + 4), 4, 4);
        c.setFont(monoFont(11));
        c.text(ex.cur + QStringLiteral(" / ") + ex.dur, 40, py + 24, Align::Left, ink2);
    }
    if (showNote) {
        const QString n = ex.noteText.trimmed();
        c.setFont(env.fs.regular, 11, familyFor(env.fs, n));
        c.text(ellipsize(c, n, 460), 40, env.canvasH - 36, Align::Left, ink2);
    }
    return need;
}

// ---------- template table, fitting --------------------------------------------------------

struct TemplateImpl
{
    double coverMax, coverMin;
    bool center;
    double (*layout)(Ctx &, Env &, const Params &, bool);
};

const TemplateImpl &impl(Template t)
{
    static const TemplateImpl compact{80, 0, true, layoutCompact};
    static const TemplateImpl classic{540, 240, false, layoutClassic};
    static const TemplateImpl immersive{0, 0, false, layoutImmersive};
    static const TemplateImpl depth{0, 0, false, layoutDepth};
    static const TemplateImpl quiet{244, 128, false, layoutQuiet};
    switch (t) {
    case Template::Classic: return classic;
    case Template::Immersive: return immersive;
    case Template::Depth: return depth;
    case Template::Quiet: return quiet;
    default: return compact;
    }
}

double ratioHeight(Ratio r)
{
    switch (r) {
    case Ratio::R4x5: return 675;
    case Ratio::R9x16: return 960;
    case Ratio::R1x1: return 540;
    default: return 0;
    }
}

// A fixed ratio can't hold everything: drop the unfocused group farthest from the first focused one.
QList<GroupRef> dropGroup(const QList<GroupRef> &groups, const QSet<int> &focus)
{
    int anchor = 0;
    for (const auto &g : groups)
        if (focus.contains(g.idx)) {
            anchor = g.idx;
            break;
        }
    int worst = -1;
    double dist = -1;
    for (int i = 0; i < groups.size(); ++i) {
        if (focus.contains(groups[i].idx))
            continue;
        const double d = std::abs(groups[i].idx - anchor) + (groups[i].idx > anchor ? 0.5 : 0);
        if (d > dist) {
            dist = d;
            worst = i;
        }
    }
    if (worst < 0)
        worst = int(groups.size()) - 1;
    QList<GroupRef> out = groups;
    out.removeAt(worst);
    return out;
}

} // namespace

const TemplateInfo &info(Template t)
{
    static const QList<Ratio> all{Ratio::Auto, Ratio::R4x5, Ratio::R9x16, Ratio::R1x1};
    static const QList<Ratio> photo{Ratio::R9x16, Ratio::R4x5, Ratio::R1x1};
    static const TemplateInfo compact{"compact", "Compact", all, Ratio::Auto, true, false, false, true, false};
    static const TemplateInfo classic{"classic", "Classic", all, Ratio::Auto, true, false, false, false, false};
    static const TemplateInfo immersive{"immersive", "Immersive", photo, Ratio::R9x16, false, true, false, true, false};
    static const TemplateInfo depth{"depth", "Depth", photo, Ratio::R9x16, false, true, true, true, false};
    static const TemplateInfo quiet{"quiet", "Whitespace", all, Ratio::Auto, true, false, false, true, true};
    switch (t) {
    case Template::Classic: return classic;
    case Template::Immersive: return immersive;
    case Template::Depth: return depth;
    case Template::Quiet: return quiet;
    default: return compact;
    }
}

Ratio ratioFor(Template t, Ratio wanted)
{
    const TemplateInfo &i = info(t);
    return i.ratios.contains(wanted) ? wanted : i.defaultRatio;
}

QImage blurred(const QImage &src, int width, int height, double sigma)
{
    return blurNow(src, width, height, sigma);
}

QImage defaultCover()
{
    // 1080×1080 sample cover: night-blue sky, a moon, a lone pole and wires.
    QImage img(540, 540, QImage::Format_ARGB32_Premultiplied);
    QPainter g(&img);
    g.setRenderHint(QPainter::Antialiasing);
    g.scale(5.4, 5.4);
    g.fillRect(QRectF(0, 0, 100, 100), QColor(0x23, 0x38, 0x9C));
    g.setPen(Qt::NoPen);
    g.setBrush(QColor(0xEE, 0xF1, 0xFA));
    g.drawEllipse(QPointF(70, 28), 8.5, 8.5);
    g.setBrush(Qt::NoBrush);
    QPainterPath w1(QPointF(0, 56));
    w1.quadTo(44, 63, 100, 52);
    g.strokePath(w1, QPen(QColor(0x0F, 0x1F, 0x6B), 0.35));
    QPainterPath w2(QPointF(0, 59));
    w2.quadTo(44, 66, 100, 55);
    g.strokePath(w2, QPen(QColor(0x0F, 0x1F, 0x6B), 0.3));
    g.fillRect(QRectF(23.2, 44, 1.6, 38), QColor(0x0F, 0x1F, 0x6B));
    g.fillRect(QRectF(19.5, 48, 9, 1), QColor(0x0F, 0x1F, 0x6B));
    g.fillRect(QRectF(0, 82, 100, 18), QColor(0x1B, 0x2E, 0x86));
    g.fillRect(QRectF(0, 81.6, 100, 0.5), QColor(0x5D, 0xAE, 0xE6, 191));
    g.fillRect(QRectF(62, 77, 1.6, 2.2), QColor(0xE9, 0xBE, 0x6A));
    return img;
}

CardResult render(const CardState &state, const CardAssets &assets, double scale, qint64 maxPixels)
{
    const Template t = state.tmpl;
    const TemplateImpl &T = impl(t);
    const Ratio ratio = ratioFor(t, state.ratio);
    const double H = ratioHeight(ratio);

    QList<GroupRef> groups;
    for (int i = 0; i < state.groups.size(); ++i)
        groups.append({i, state.groups[i]});

    const auto &an = assets.analysis;
    const QColor seed = state.seed.isValid() ? state.seed
                        : an.swatches.isEmpty() ? QColor(FallbackSeed)
                                                : an.swatches[std::clamp(an.defaultIndex, 0, int(an.swatches.size()) - 1)].color;
    Env env{state,
            assets,
            assets.cover.isNull() ? defaultCover() : assets.cover,
            fontSet(state),
            cardPalette(seed, an.accent, state.style, state.mode, an.meanL),
            resolveDark(state.mode, an.meanL),
            groups,
            !state.focus.isEmpty(),
            H,
            0,
            scale >= 2, // exports: don't keep big blur layers in the preview cache
            {},
            &info(t)};

    // Solve: cover size, type scale and which groups fit a fixed ratio.
    Measurer measurer;
    Ctx mc;
    mc.dev = &measurer.dev;
    const double base = std::clamp(state.scale, 0.6, 1.5);
    Params p{base, T.coverMax, groups, 0};
    double h = T.layout(mc, env, p, false);
    int dropped = 0;
    if (H > 0) {
        for (int guard = 0; h > H + 0.5 && guard < 120; ++guard) {
            if (T.coverMin > 0 && p.cover > T.coverMin)
                p.cover = std::max(T.coverMin, p.cover - 24);
            else if (p.fs > base * 0.72)
                p.fs = std::max(base * 0.72, p.fs * 0.95);
            else if (p.groups.size() > 1) {
                p.groups = dropGroup(p.groups, state.focus);
                ++dropped;
            } else
                break;
            env.hits.clear();
            h = T.layout(mc, env, p, false);
        }
        if (T.center && h < H)
            p.off = (H - h) / 2;
        h = H;
    }
    h = std::ceil(h);

    double k = scale;
    if (W * k * h * k > double(maxPixels))
        k = std::sqrt(double(maxPixels) / (W * h));
    QImage img = makeImage(std::max(1, int(std::lround(W * k))), std::max(1, int(std::lround(h * k))));
    k = img.width() / W;

    env.canvasH = h;
    env.hits.clear();
    {
        QPainter painter(&img);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
        painter.scale(k, k);
        Ctx c;
        c.p = &painter;
        c.dev = &img;
        c.k = k;
        T.layout(c, env, p, true);
    }

    CardResult r;
    r.image = img;
    r.height = h;
    r.scale = k;
    r.hits = env.hits;
    r.dropped = dropped;
    r.ratio = ratio;
    return r;
}

} // namespace Cards
