// SPDX-License-Identifier: AGPL-3.0-or-later
// Drawing follows Folia's Pendolo (AGPL-3.0), pendoloClockworkDraw2d.ts.
#include "ClockworkPart.h"

#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

#include <cmath>

namespace {

constexpr double Tau = 6.283185307179586;

// Trapezoidal teeth around a circle.
QPainterPath gearPath(double radius, int teeth, double depth)
{
    QPainterPath path;
    const double inner = radius - depth, outer = radius;
    const double per = Tau / std::max(3, teeth);
    for (int i = 0; i < teeth; ++i) {
        const double b = i * per;
        const double a[4] = {b - per * 0.22, b - per * 0.12, b + per * 0.12, b + per * 0.22};
        const double r[4] = {inner, outer, outer, inner};
        for (int k = 0; k < 4; ++k) {
            const QPointF p(r[k] * std::cos(a[k]), r[k] * std::sin(a[k]));
            if (i == 0 && k == 0)
                path.moveTo(p);
            else
                path.lineTo(p);
        }
    }
    path.closeSubpath();
    return path;
}

void circle(QPainter *p, double r)
{
    p->drawEllipse(QPointF(0, 0), r, r);
}

void spokedWheel(QPainter *p, double hub, double rim, int spokes, const QPen &pen)
{
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    circle(p, hub);
    circle(p, rim);
    const double step = Tau / spokes;
    for (int i = 0; i < spokes; ++i) {
        const double a = i * step;
        p->drawLine(QPointF(hub * std::cos(a), hub * std::sin(a)), QPointF(rim * std::cos(a), rim * std::sin(a)));
    }
    const double mid = (hub + rim) / 2, hole = (rim - hub) * 0.22;
    for (int i = 0; i < spokes; ++i) {
        const double a = i * step + step / 2;
        p->drawEllipse(QPointF(mid * std::cos(a), mid * std::sin(a)), hole, hole);
    }
}

} // namespace

ClockworkPart::ClockworkPart(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setMipmap(true);
    connect(this, &QQuickItem::widthChanged, this, [this] { update(); });
}

void ClockworkPart::paint(QPainter *p)
{
    p->setRenderHint(QPainter::Antialiasing);
    p->translate(width() / 2, height() / 2);
    const double R = m_radius;
    const double lw = m_lineWidth;

    if (m_kind == u"glow") {
        // Darkens the area around the wheel so lines and gears read on any background.
        QRadialGradient g(QPointF(0, 0), R);
        QColor c = m_fill;
        auto at = [&c](double a) { QColor x = c; x.setAlphaF(float(a)); return x; };
        g.setColorAt(0, at(0.72));
        g.setColorAt(0.35, at(0.52));
        g.setColorAt(0.7, at(0.2));
        g.setColorAt(1, at(0));
        p->setPen(Qt::NoPen);
        p->setBrush(g);
        circle(p, R);
        return;
    }

    if (m_kind == u"dial") {
        // Guide rings and 60 technical ticks (R here is the main wheel's base radius).
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(m_subtle, 1));
        for (double f : {0.3, 0.6, 0.85, 1.15, 1.4})
            circle(p, R * f);
        const double tickR = R * 1.15;
        for (int i = 0; i < 60; ++i) {
            const double a = i * Tau / 60;
            const bool major = i % 5 == 0;
            const double len = major ? R * 0.028 : R * 0.014;
            p->setPen(QPen(major ? m_accent : m_subtle, major ? lw * 1.1 : lw * 0.7));
            p->drawLine(QPointF(tickR * std::cos(a), tickR * std::sin(a)),
                        QPointF((tickR + len) * std::cos(a), (tickR + len) * std::sin(a)));
        }
        return;
    }

    if (m_kind == u"hairspring") {
        // Spiral from 0.56 to 0.88 of the radius, 3.5 coils.
        QPainterPath path;
        const double coils = 3.5, start = R * 0.56, end = R * 0.88;
        const int steps = int(coils * 60);
        for (int i = 0; i <= steps; ++i) {
            const double t = double(i) / steps;
            const double a = t * coils * Tau;
            const double r = start + (end - start) * std::pow(t, 0.9);
            const QPointF pt(r * std::cos(a), r * std::sin(a));
            if (i == 0)
                path.moveTo(pt);
            else
                path.lineTo(pt);
        }
        p->strokePath(path, QPen(m_stroke, lw * 0.6));
        return;
    }

    if (m_kind == u"main") {
        // Escapement wheel: 36 teeth on the rim, 6 spokes, bevel rings, guilloche, rivets.
        // R = base radius; the teeth reach R + toothDepth.
        p->setPen(QPen(m_accent, lw * 1.25));
        p->setBrush(m_fill);
        p->drawPath(gearPath(R + m_toothDepth * 0.8, 36, m_toothDepth));
        spokedWheel(p, R * 0.2, R * 0.85, 6, QPen(m_stroke, lw));
        p->setPen(QPen(m_subtle, lw * 0.45));
        p->setBrush(Qt::NoBrush);
        circle(p, R * 0.88);
        circle(p, R * 0.92);
        const double gi = R * 0.62, go = R * 0.83;
        for (int i = 0; i < 48; ++i) {
            const double a = i * Tau / 48;
            const double in = i % 2 == 0 ? gi : gi + (go - gi) * 0.3;
            QColor c = m_subtle;
            c.setAlphaF(c.alphaF() * (i % 2 == 0 ? 0.6f : 0.4f));
            p->setPen(QPen(c, lw * 0.4));
            p->drawLine(QPointF(in * std::cos(a), in * std::sin(a)), QPointF(go * std::cos(a), go * std::sin(a)));
        }
        p->setPen(QPen(m_subtle, lw * 0.45));
        for (int i = 0; i < 12; ++i) {
            const double a = i * Tau / 12;
            p->drawEllipse(QPointF(R * 0.96 * std::cos(a), R * 0.96 * std::sin(a)), R * 0.0055 + 1, R * 0.0055 + 1);
        }
        return;
    }

    // Generic gear (R = tip radius).
    p->setPen(QPen(m_stroke, lw));
    p->setBrush(m_fill);
    p->drawPath(gearPath(R, m_teeth, m_toothDepth));
    p->setBrush(Qt::NoBrush);
    if (m_spokes > 0)
        spokedWheel(p, R * 0.26, R * 0.82, m_spokes, QPen(m_subtle, lw * 0.85));
    if (m_geneva) {
        // Parallel "Côtes de Genève" stripes, clipped to the gear face.
        p->save();
        QPainterPath clip;
        clip.addEllipse(QPointF(0, 0), R * 0.8, R * 0.8);
        p->setClipPath(clip);
        const double span = R * 1.6, step = span / 10;
        for (int i = 1; i <= 9; ++i) {
            QColor c = m_subtle;
            c.setAlphaF(c.alphaF() * (i % 2 == 0 ? 0.5f : 0.35f));
            p->setPen(QPen(c, lw * 0.4));
            const double y = -span / 2 + i * step;
            p->drawLine(QPointF(-R, y), QPointF(R, y));
        }
        p->restore();
    }
    // Jewel bearing.
    p->setPen(QPen(m_accent, lw * 0.6));
    circle(p, std::max(2.0, R * 0.06));
}
