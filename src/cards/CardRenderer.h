// SPDX-License-Identifier: AGPL-3.0-or-later
// Lyric card rendering with QPainter. The five templates, the fitting of fixed ratios and
// the drawing helpers are ported from spacedge/lyric-card-generator (AGPL-3.0),
// src/render/{index,templates,common}.js. Everything is laid out on a 540-wide design
// canvas and drawn at any scale (2 = 1080 px, 4 = 2160 px wide).
#pragma once

#include "CardColor.h"

#include <QImage>
#include <QList>
#include <QRectF>
#include <QSet>
#include <QString>

#include <optional>

namespace Cards {

inline constexpr double DesignWidth = 540;

enum class Template { Compact, Classic, Immersive, Depth, Quiet };
enum class Ratio { Auto, R4x5, R9x16, R1x1 };
enum class BlurLevel { Light, Mid, Heavy };
enum class FontPreset { Sans, Serif, App };

struct Group
{
    QString original;
    QString translation;
};

struct Extras
{
    bool date = false;
    QString dateText;
    bool note = false;
    QString noteText;
    bool progress = false;
    QString cur; // "1:52"
    QString dur; // "4:05"
};

struct CardState
{
    Template tmpl = Template::Compact;
    QString title;
    QString artist;
    QList<Group> groups;
    QSet<int> focus;          // indices into groups: highlighted lines
    QColor seed;              // invalid: the cover's default swatch
    CardColor::Style style = CardColor::Style::Content;
    CardColor::Mode mode = CardColor::Mode::Auto;
    BlurLevel blur = BlurLevel::Mid;
    std::optional<double> depthX; // 景深 framing, 0..1 across the cover
    FontPreset font = FontPreset::Sans;
    QString appFont = QStringLiteral("Noto Sans");
    double scale = 1.0;       // text size, 0.6..1.5
    bool alignCenter = false;
    bool showTranslation = true;
    Ratio ratio = Ratio::Auto;
    Extras extras;
};

struct CardAssets
{
    QImage cover; // empty: a built-in sample cover is used
    CardColor::CoverAnalysis analysis = CardColor::fallbackAnalysis();
};

struct Hit
{
    int index; // group index
    QRectF rect; // design px
};

struct CardResult
{
    QImage image;
    double height = 0; // design px
    double scale = 1;  // device px per design px
    QList<Hit> hits;
    int dropped = 0;   // groups that didn't fit a fixed ratio
    Ratio ratio = Ratio::Auto;
};

struct TemplateInfo
{
    const char *id;
    const char *label;
    QList<Ratio> ratios;
    Ratio defaultRatio;
    bool style, blur, crop, progress, note;
};
const TemplateInfo &info(Template t);
Ratio ratioFor(Template t, Ratio wanted);

// scale: device px per design px. maxPixels caps the image area (long cards at 4x).
CardResult render(const CardState &state, const CardAssets &assets, double scale, qint64 maxPixels = 16'000'000);

QImage defaultCover();

// Gaussian-like blur (three box passes, done at reduced size for large sigmas). Public for tests.
QImage blurred(const QImage &src, int width, int height, double sigma);

} // namespace Cards
