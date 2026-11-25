// SPDX-License-Identifier: AGPL-3.0-or-later
// Colour logic for lyric cards, ported from spacedge/lyric-card-generator (AGPL-3.0),
// src/color.js: OKLab / OKLCH conversions, tone palettes, WCAG contrast, cover analysis
// and the card palette (four styles, light / dark, contrast guarantees).
#pragma once

#include <QColor>
#include <QImage>
#include <QList>
#include <QPointF>
#include <QString>

#include <map>

namespace CardColor {

inline const QString FallbackSeed = QStringLiteral("#5DAEE6");

struct Oklch
{
    double L = 0, C = 0, H = 0;
};

Oklch toOklch(const QColor &c);
QColor fromHex(const QString &hex);
QString toHex(const QColor &c);

// Colour of tone T (CIELAB L*, 0–100) for hue H with at most chroma C, inside sRGB.
QColor toneColor(double H, double C, double T);

// A fixed (H, C) palette: tone -> colour, cached.
class Palette
{
public:
    Palette(double H, double C) : m_H(H), m_C(C) {}
    QColor operator()(double tone);

private:
    double m_H, m_C;
    std::map<int, QColor> m_cache; // tone * 100
};

double luminance(const QColor &c);          // WCAG relative luminance
double contrast(const QColor &a, const QColor &b); // 1..21
double toneOf(const QColor &c);             // CIELAB L*
double hueDistance(double h1, double h2);   // 0..180
// Walks the palette from `tone` in direction dir (+1 lighter / -1 darker) until the contrast
// against bg reaches `min` (or the tone hits 0 / 100).
QColor ensureContrast(Palette &pal, double tone, const QColor &bg, double min, int dir);
double okDistance(const QColor &a, const QColor &b); // OKLab euclidean distance

struct Swatch
{
    QColor color;
    double L = 0, C = 0, H = 0, share = 0;
    QString role; // base, accent, vivid, neutral
};

struct CoverAnalysis
{
    QList<Swatch> swatches; // 1..6, [0] is the base
    int defaultIndex = 0;
    QColor accent;          // invalid if the cover has none
    double meanL = 0.5;
    QPointF focus{0.5, 0.5};
    QColor edgeTop, edgeBottom;
};

CoverAnalysis fallbackAnalysis();
// Deterministic analysis of a cover (cropped square, sampled at 96 px).
CoverAnalysis analyzeCover(const QImage &cover);

enum class Style { Content, Tonal, Vibrant, Mono }; // 忠实 柔和 鲜明 单色
enum class Mode { Auto, Light, Dark };

bool resolveDark(Mode mode, double meanL);

struct CardPalette
{
    QColor bg, container, on, onVar, accent, track, shadow;
    bool isDark = true;
};

CardPalette cardPalette(const QColor &seed, const QColor &accent, Style style, Mode mode, double meanL);

QColor withAlpha(QColor c, double a);

} // namespace CardColor
