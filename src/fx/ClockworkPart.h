// SPDX-License-Identifier: AGPL-3.0-or-later
// One wireframe clockwork part for the Pendolo style, painted once with QPainter and then
// only rotated / moved by the GPU (QML `rotation`), so animating gears costs no repainting.
// The drawing follows Folia's Pendolo Canvas2D path (AGPL-3.0, chthollyphile/folia-major,
// src/components/visualizer/pendolo/pendoloClockworkDraw2d.ts).
//
// The item is square; its centre is the part's axis. `unit` is the main wheel's base
// radius in px; all sizes are fractions of it, as in the original.
#pragma once

#include <QColor>
#include <QQuickPaintedItem>

class ClockworkPart : public QQuickPaintedItem
{
    Q_OBJECT
    // main | dial | gear | hairspring | glow
    Q_PROPERTY(QString kind READ kind WRITE setKind NOTIFY changed)
    Q_PROPERTY(double radius READ radius WRITE setRadius NOTIFY changed)   // px: gear tip / dial size
    Q_PROPERTY(int teeth READ teeth WRITE setTeeth NOTIFY changed)
    Q_PROPERTY(double toothDepth READ toothDepth WRITE setToothDepth NOTIFY changed) // px
    Q_PROPERTY(int spokes READ spokes WRITE setSpokes NOTIFY changed)       // 0 = none
    Q_PROPERTY(bool geneva READ geneva WRITE setGeneva NOTIFY changed)
    Q_PROPERTY(double lineWidth READ lineWidth WRITE setLineWidth NOTIFY changed)
    Q_PROPERTY(QColor stroke READ stroke WRITE setStroke NOTIFY changed)
    Q_PROPERTY(QColor fill READ fill WRITE setFill NOTIFY changed)
    Q_PROPERTY(QColor accent READ accent WRITE setAccent NOTIFY changed)
    Q_PROPERTY(QColor subtle READ subtle WRITE setSubtle NOTIFY changed)

public:
    explicit ClockworkPart(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;

#define CW_PROP(Type, name, Name)                                                                                      \
    Type name() const { return m_##name; }                                                                             \
    void set##Name(const Type &v)                                                                                      \
    {                                                                                                                  \
        if (m_##name == v)                                                                                             \
            return;                                                                                                    \
        m_##name = v;                                                                                                  \
        emit changed();                                                                                                \
        update();                                                                                                      \
    }
    CW_PROP(QString, kind, Kind)
    CW_PROP(double, radius, Radius)
    CW_PROP(int, teeth, Teeth)
    CW_PROP(double, toothDepth, ToothDepth)
    CW_PROP(int, spokes, Spokes)
    CW_PROP(bool, geneva, Geneva)
    CW_PROP(double, lineWidth, LineWidth)
    CW_PROP(QColor, stroke, Stroke)
    CW_PROP(QColor, fill, Fill)
    CW_PROP(QColor, accent, Accent)
    CW_PROP(QColor, subtle, Subtle)
#undef CW_PROP

signals:
    void changed();

private:
    QString m_kind = QStringLiteral("gear");
    double m_radius = 50;
    int m_teeth = 20;
    double m_toothDepth = 6;
    int m_spokes = 0;
    bool m_geneva = false;
    double m_lineWidth = 1.5;
    QColor m_stroke = Qt::white;
    QColor m_fill = Qt::transparent;
    QColor m_accent = Qt::white;
    QColor m_subtle = QColor(255, 255, 255, 40);
};
