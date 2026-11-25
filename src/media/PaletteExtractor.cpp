// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PaletteExtractor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace {

struct Rgb
{
    float r = 0, g = 0, b = 0;
};

struct Cluster
{
    Rgb center;
    int count = 0;
    QColor color() const { return QColor::fromRgbF(center.r, center.g, center.b); }
};

float luma(const Rgb &c)
{
    return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
}

float dist2(const Rgb &a, const Rgb &b)
{
    const float dr = a.r - b.r, dg = a.g - b.g, db = a.b - b.b;
    return dr * dr + dg * dg + db * db;
}

struct Hsl
{
    float h = 0, s = 0, l = 0;
};

Hsl hsl(const QColor &c)
{
    float h = c.hslHueF();
    return {h < 0 ? 0.0f : h, c.hslSaturationF(), c.lightnessF()};
}

QColor fromHsl(float h, float s, float l)
{
    return QColor::fromHslF(std::clamp(h, 0.0f, 1.0f), std::clamp(s, 0.0f, 1.0f), std::clamp(l, 0.0f, 1.0f));
}

double relativeLuminance(const QColor &c)
{
    auto ch = [](double v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * ch(c.redF()) + 0.7152 * ch(c.greenF()) + 0.0722 * ch(c.blueF());
}

} // namespace

PaletteColors PaletteColors::defaults()
{
    PaletteColors p;
    p.dominant = QColor(0x3a, 0x2f, 0x5c);
    p.vibrant = QColor(0xff, 0x5f, 0x8f);
    p.muted = QColor(0x6b, 0x63, 0x85);
    p.dark = QColor(0x14, 0x12, 0x1c);
    p.light = QColor(0xe8, 0xe0, 0xff);
    p.accent = QColor(0xff, 0x7a, 0xa2);
    p.text = QColor(0xf6, 0xf3, 0xff);
    return p;
}

namespace PaletteExtractor {

double contrastRatio(const QColor &a, const QColor &b)
{
    const double la = relativeLuminance(a), lb = relativeLuminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

PaletteColors extract(const QImage &image)
{
    if (image.isNull())
        return PaletteColors::defaults();

    const QImage small = image.convertToFormat(QImage::Format_ARGB32)
                             .scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    std::vector<Rgb> pixels;
    pixels.reserve(64 * 64);
    for (int y = 0; y < small.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(small.constScanLine(y));
        for (int x = 0; x < small.width(); ++x) {
            const QRgb p = line[x];
            if (qAlpha(p) < 128)
                continue;
            pixels.push_back({qRed(p) / 255.0f, qGreen(p) / 255.0f, qBlue(p) / 255.0f});
        }
    }
    if (pixels.empty())
        return PaletteColors::defaults();

    // k-means, seeded at luminance quantiles so the result is deterministic.
    constexpr int K = 6;
    std::vector<Rgb> sorted = pixels;
    std::sort(sorted.begin(), sorted.end(), [](const Rgb &a, const Rgb &b) { return luma(a) < luma(b); });
    std::array<Cluster, K> clusters;
    for (int k = 0; k < K; ++k)
        clusters[k].center = sorted[std::min(sorted.size() - 1, size_t((k + 0.5) / K * sorted.size()))];

    std::vector<int> assignment(pixels.size(), 0);
    for (int iter = 0; iter < 12; ++iter) {
        std::array<Rgb, K> sums{};
        std::array<int, K> counts{};
        for (size_t i = 0; i < pixels.size(); ++i) {
            int best = 0;
            float bestD = dist2(pixels[i], clusters[0].center);
            for (int k = 1; k < K; ++k) {
                const float d = dist2(pixels[i], clusters[k].center);
                if (d < bestD) {
                    bestD = d;
                    best = k;
                }
            }
            assignment[i] = best;
            sums[best].r += pixels[i].r;
            sums[best].g += pixels[i].g;
            sums[best].b += pixels[i].b;
            ++counts[best];
        }
        for (int k = 0; k < K; ++k) {
            clusters[k].count = counts[k];
            if (counts[k] > 0)
                clusters[k].center = {sums[k].r / counts[k], sums[k].g / counts[k], sums[k].b / counts[k]};
        }
    }
    std::sort(clusters.begin(), clusters.end(), [](const Cluster &a, const Cluster &b) { return a.count > b.count; });
    const float total = float(pixels.size());

    PaletteColors p;
    p.dominant = clusters[0].color();

    // Vibrant: saturated, mid lightness, with some presence.
    const Cluster *vibrant = nullptr;
    float bestScore = 0;
    for (const auto &c : clusters) {
        if (c.count == 0)
            continue;
        const Hsl h = hsl(c.color());
        const float score = h.s * std::max(0.0f, 1.0f - std::abs(h.l - 0.5f) * 1.6f)
                            * (0.3f + 0.7f * std::sqrt(c.count / total));
        if (h.s > 0.2f && score > bestScore) {
            bestScore = score;
            vibrant = &c;
        }
    }
    p.vibrant = vibrant ? vibrant->color() : p.dominant;

    // Muted: low-ish saturation, mid lightness, most common.
    const Cluster *muted = nullptr;
    for (const auto &c : clusters) {
        const Hsl h = hsl(c.color());
        if (c.count > 0 && h.s >= 0.04f && h.s <= 0.45f && h.l >= 0.25f && h.l <= 0.75f) {
            muted = &c;
            break; // clusters are sorted by count
        }
    }
    {
        const Hsl d = hsl(p.dominant);
        p.muted = muted ? muted->color() : fromHsl(d.h, d.s * 0.4f, 0.45f);
    }

    const Hsl dom = hsl(p.dominant);
    const Hsl vib = hsl(p.vibrant);
    const float baseHue = dom.s < 0.1f ? vib.h : dom.h;
    const float baseSat = dom.s < 0.1f ? vib.s : dom.s;

    p.dark = fromHsl(baseHue, std::min(baseSat, 0.55f) * 0.8f, 0.10f);
    p.light = fromHsl(vib.h, std::min(vib.s, 0.6f), 0.86f);
    p.accent = vib.s < 0.12f ? fromHsl(vib.h, 0.0f, 0.82f) : fromHsl(vib.h, std::clamp(vib.s, 0.45f, 0.9f), 0.68f);
    p.text = fromHsl(vib.h, std::min(vib.s, 0.25f), 0.96f);
    if (contrastRatio(p.text, p.dark) < 7.0)
        p.text = Qt::white;
    return p;
}

} // namespace PaletteExtractor

Palette::Palette(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<PaletteColors>();
}

void Palette::setColors(const PaletteColors &colors)
{
    if (m_colors == colors)
        return;
    m_colors = colors;
    emit changed();
}
