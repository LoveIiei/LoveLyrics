// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <QColor>
#include <QImage>
#include <QObject>

struct PaletteColors
{
    QColor dominant; // most common colour
    QColor vibrant;  // most saturated, not too dark/bright
    QColor muted;    // calm mid colour
    QColor dark;     // background tint, always dark
    QColor light;    // light tint of the vibrant hue
    QColor accent;   // vibrant hue, bright enough to read on `dark`
    QColor text;     // readable on `dark` (contrast >= 7:1)

    static PaletteColors defaults();
    bool operator==(const PaletteColors &) const = default;
};
Q_DECLARE_METATYPE(PaletteColors)

namespace PaletteExtractor {

// k-means on a 64x64 thumbnail. Deterministic for a given image.
PaletteColors extract(const QImage &image);

// WCAG contrast ratio, 1..21.
double contrastRatio(const QColor &a, const QColor &b);

} // namespace PaletteExtractor

// The current track's palette for QML. Views animate the change themselves.
class Palette : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QColor dominant READ dominant NOTIFY changed)
    Q_PROPERTY(QColor vibrant READ vibrant NOTIFY changed)
    Q_PROPERTY(QColor muted READ muted NOTIFY changed)
    Q_PROPERTY(QColor dark READ dark NOTIFY changed)
    Q_PROPERTY(QColor light READ light NOTIFY changed)
    Q_PROPERTY(QColor accent READ accent NOTIFY changed)
    Q_PROPERTY(QColor text READ text NOTIFY changed)

public:
    explicit Palette(QObject *parent = nullptr);

    void setColors(const PaletteColors &colors);
    const PaletteColors &colors() const { return m_colors; }

    QColor dominant() const { return m_colors.dominant; }
    QColor vibrant() const { return m_colors.vibrant; }
    QColor muted() const { return m_colors.muted; }
    QColor dark() const { return m_colors.dark; }
    QColor light() const { return m_colors.light; }
    QColor accent() const { return m_colors.accent; }
    QColor text() const { return m_colors.text; }

signals:
    void changed();

private:
    PaletteColors m_colors = PaletteColors::defaults();
};
