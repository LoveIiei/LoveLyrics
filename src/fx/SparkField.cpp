// SPDX-License-Identifier: AGPL-3.0-or-later
// Burst kinds and their parameters are ported from Folia (AGPL-3.0,
// chthollyphile/folia-major), src/components/visualizer/hanabi/mg/burstPlan.ts:
//   p(τ) = p0 + v0·(1 − e^{−kτ})/k + vt·(τ − (1 − e^{−kτ})/k)
#include "SparkField.h"

#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>

namespace {

constexpr float Tau = 6.2831853f;
constexpr float GoldenAngle = 2.399963229728653f;
constexpr int MaxQuads = 12000;

// ---- colours -----------------------------------------------------------------------------

QRgb mixRgb(QRgb a, QRgb b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    auto ch = [t](int x, int y) { return int(std::lround(x + (y - x) * t)); };
    return qRgba(ch(qRed(a), qRed(b)), ch(qGreen(a), qGreen(b)), ch(qBlue(a), qBlue(b)), 255);
}
const QRgb White = qRgb(255, 255, 255);
const QRgb Gold = qRgb(255, 205, 110);
const QRgb Ember = qRgb(255, 120, 60);

quint32 hash32(quint32 x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}
float hash01(quint32 a, quint32 b)
{
    return float(hash32(a * 0x9e3779b9U ^ hash32(b))) / 4294967296.0f;
}

// ---- material ----------------------------------------------------------------------------

class SparkShader : public QSGMaterialShader
{
public:
    SparkShader()
    {
        setShaderFileName(VertexStage, QStringLiteral(":/shaders/spark.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/shaders/spark.frag.qsb"));
    }
    bool updateUniformData(RenderState &state, QSGMaterial *, QSGMaterial *) override
    {
        QByteArray *buf = state.uniformData();
        bool changed = false;
        if (state.isMatrixDirty()) {
            const QMatrix4x4 m = state.combinedMatrix();
            std::memcpy(buf->data(), m.constData(), 64);
            changed = true;
        }
        if (state.isOpacityDirty()) {
            const float o = state.opacity();
            std::memcpy(buf->data() + 64, &o, 4);
            changed = true;
        }
        return changed;
    }
};

class SparkMaterial : public QSGMaterial
{
public:
    SparkMaterial() { setFlag(Blending); }
    QSGMaterialType *type() const override
    {
        static QSGMaterialType t;
        return &t;
    }
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override { return new SparkShader; }
    int compare(const QSGMaterial *) const override { return 0; }
};

struct Vertex
{
    float x, y;
    float u, v, shape;
    quint8 r, g, b, a;
};

const QSGGeometry::AttributeSet &attributes()
{
    static const QSGGeometry::Attribute attrs[] = {
        QSGGeometry::Attribute::createWithAttributeType(0, 2, QSGGeometry::FloatType, QSGGeometry::PositionAttribute),
        QSGGeometry::Attribute::createWithAttributeType(1, 3, QSGGeometry::FloatType, QSGGeometry::TexCoordAttribute),
        QSGGeometry::Attribute::createWithAttributeType(2, 4, QSGGeometry::UnsignedByteType, QSGGeometry::ColorAttribute),
    };
    static const QSGGeometry::AttributeSet set = {3, sizeof(Vertex), attrs};
    return set;
}

// Writes quads into a vertex buffer.
struct QuadWriter
{
    std::vector<Vertex> &out;
    int quads = 0;

    bool full() const { return quads >= MaxQuads; }

    void color(Vertex &v, QRgb c, float a)
    {
        v.r = quint8(qRed(c));
        v.g = quint8(qGreen(c));
        v.b = quint8(qBlue(c));
        v.a = quint8(std::clamp(a, 0.0f, 1.0f) * 255);
    }
    // A glow dot of radius r (the glow fills the quad).
    void dot(float x, float y, float r, QRgb c, float a)
    {
        if (full() || a <= 0.004f || r <= 0.1f)
            return;
        Vertex q[4];
        const float xs[4] = {x - r, x + r, x + r, x - r};
        const float ys[4] = {y - r, y - r, y + r, y + r};
        const float us[4] = {-1, 1, 1, -1};
        const float vs[4] = {-1, -1, 1, 1};
        for (int i = 0; i < 4; ++i) {
            q[i] = {xs[i], ys[i], us[i], vs[i], 0, 0, 0, 0, 0};
            color(q[i], c, a);
        }
        push(q);
    }
    // A trail segment from tail (x0, y0) to head (x1, y1); along0/along1 = brightness ramp.
    void segment(float x0, float y0, float x1, float y1, float width, float along0, float along1, QRgb c, float a)
    {
        if (full() || a <= 0.004f)
            return;
        float dx = x1 - x0, dy = y1 - y0;
        const float len = std::hypot(dx, dy);
        if (len < 0.3f)
            return;
        const float nx = -dy / len * width, ny = dx / len * width;
        Vertex q[4] = {
            {x0 + nx, y0 + ny, along0, -1, 1, 0, 0, 0, 0},
            {x1 + nx, y1 + ny, along1, -1, 1, 0, 0, 0, 0},
            {x1 - nx, y1 - ny, along1, 1, 1, 0, 0, 0, 0},
            {x0 - nx, y0 - ny, along0, 1, 1, 0, 0, 0, 0},
        };
        for (auto &v : q)
            color(v, c, a);
        push(q);
    }
    void push(const Vertex (&q)[4])
    {
        out.push_back(q[0]);
        out.push_back(q[1]);
        out.push_back(q[2]);
        out.push_back(q[0]);
        out.push_back(q[2]);
        out.push_back(q[3]);
        ++quads;
    }
};

// ---- burst planning ----------------------------------------------------------------------

struct Planner
{
    SparkField::Event &ev;
    std::mt19937 rng;
    float x, y, at, R, D;
    int n;
    float trailK, twinkleK, squash, rotation;
    std::vector<QRgb> colors;
    float tiltCos, tiltSin;
    float minSize;

    float rnd() { return std::uniform_real_distribution<float>(0, 1)(rng); }
    QRgb c(int i) const { return colors.empty() ? White : colors[size_t(i) % colors.size()]; }
    float life(float lo, float hi) { return D * (lo + (hi - lo) * rnd()); }
    float depthOf(float z) const { return 0.62f + 0.38f * (z + 1) / 2; }

    // Even directions on a sphere (Fibonacci), randomly tilted; returns projected x, y and depth z.
    void sphere(int i, int count, float jitter, float &dx, float &dy, float &dz)
    {
        const float z = 1 - 2 * (i + 0.5f) / float(count);
        const float r = std::sqrt(std::max(0.0f, 1 - z * z));
        const float phi = i * GoldenAngle + (rnd() - 0.5f) * jitter;
        const float px = r * std::cos(phi), py = r * std::sin(phi);
        dx = px;
        dy = py * tiltCos - z * tiltSin;
        dz = py * tiltSin + z * tiltCos;
    }
    void planar(float px, float py, float &dx, float &dy) const
    {
        const float sy = py * squash;
        dx = px * std::cos(rotation) - sy * std::sin(rotation);
        dy = px * std::sin(rotation) + sy * std::cos(rotation);
    }

    struct S
    {
        float x0 = 0, y0 = 0, vx = 0, vy = 0, drag = 3, fall = 0, start = 0, life = 1, size = 2;
        QRgb color = White, endColor = 0;
        float shift = 1, trail = 0;
        int segments = 0;
        float fadeTail = 0.3f, strobe = 0, twinkle = 0, depth = 1;
    };
    void add(const S &s)
    {
        const float lifeLeft = std::min(s.life, D - s.start - 0.02f);
        if (lifeLeft <= 0.05f)
            return;
        SparkField::Star star{x + s.x0, y + s.y0, s.vx, s.vy, s.drag, s.fall, at + s.start, lifeLeft,
                              std::max(minSize, s.size), s.color, s.endColor ? s.endColor : s.color, s.shift,
                              s.trail, s.trail > 0 ? std::max(1, s.segments) : 0, s.fadeTail, s.strobe, s.twinkle,
                              s.depth, quint32(rng())};
        ev.stars.push_back(star);
    }
    void flash(float fx, float fy, float delay, float size, QRgb color, float lifeS = 0.32f)
    {
        if (delay >= D)
            return;
        ev.flashes.push_back({x + fx, y + fy, at + delay, size, std::min(lifeS, D - delay), mixRgb(color, White, 0.7f)});
    }

    void plan(const QString &kind)
    {
        const QRgb c0 = c(0), c1 = c(1), c2 = c(2);
        float dx, dy, dz;
        if (kind == u"botan") {
            const int core = int(std::lround(n * 0.22)), main = n - core;
            const float k = 3.6f;
            for (int i = 0; i < main; ++i) {
                sphere(i, main, 0.12f, dx, dy, dz);
                const float sp = R * k * (0.97f + rnd() * 0.06f);
                S s;
                s.vx = dx * sp, s.vy = dy * sp, s.drag = k, s.fall = R * 0.24f, s.life = life(0.85f, 0.97f);
                s.size = R * 0.034f, s.color = c0, s.endColor = c1, s.shift = 0.48f, s.fadeTail = 0.12f;
                s.twinkle = twinkleK * 0.6f, s.depth = depthOf(dz);
                add(s);
            }
            for (int i = 0; i < core; ++i) {
                sphere(i, core, 0.12f, dx, dy, dz);
                const float sp = R * k * 0.5f;
                S s;
                s.vx = dx * sp, s.vy = dy * sp, s.drag = k, s.fall = R * 0.2f, s.life = life(0.6f, 0.72f);
                s.size = R * 0.03f, s.color = c2, s.endColor = mixRgb(c2, White, 0.5f), s.shift = 0.55f;
                s.fadeTail = 0.12f, s.depth = depthOf(dz);
                add(s);
            }
            flash(0, 0, 0, R * 1.2f, c0, 0.3f);
        } else if (kind == u"yanagi" || kind == u"kamuro") {
            const bool kamuro = kind == u"kamuro";
            const float k = kamuro ? 3.0f : 2.5f;
            const QRgb gold = mixRgb(c0, Gold, kamuro ? 0.78f : 0.6f);
            for (int i = 0; i < n; ++i) {
                sphere(i, n, kamuro ? 0.2f : 0.4f, dx, dy, dz);
                const float sp = R * k * (kamuro ? 1.05f * (0.95f + rnd() * 0.1f) : 0.72f * (0.9f + rnd() * 0.2f));
                S s;
                s.vx = dx * sp, s.vy = dy * sp - R * (kamuro ? 0.12f : 0.25f), s.drag = k;
                s.fall = R * (kamuro ? 0.55f : 0.95f), s.life = life(kamuro ? 0.9f : 0.88f, 0.99f);
                s.size = R * (kamuro ? 0.017f : 0.013f), s.color = gold, s.endColor = mixRgb(gold, Ember, 0.55f);
                s.shift = 0.58f, s.trail = (kamuro ? 0.45f + trailK * 0.8f : 0.35f + trailK * 0.95f) * 0.6f;
                s.segments = 4, s.fadeTail = kamuro ? 0.5f : 0.45f;
                s.twinkle = kamuro ? 0.55f + twinkleK * 0.45f : 0.25f + twinkleK * 0.5f;
                s.depth = depthOf(dz) * (kamuro ? 1.0f : 0.9f);
                add(s);
            }
            flash(0, 0, 0, R * (kamuro ? 1.3f : 0.8f), gold, kamuro ? 0.42f : 0.3f);
        } else if (kind == u"senrin") {
            const int flowers = std::clamp(int(std::lround(n / 14.0)), 4, 10);
            const int per = std::max(6, int(std::lround(double(n) / flowers)));
            const float k = 4.4f;
            for (int f = 0; f < flowers; ++f) {
                const float angle = f * GoldenAngle + rnd() * 0.6f;
                const float dist = R * 0.78f * std::sqrt((f + 0.5f) / flowers);
                const float ox = std::cos(angle) * dist, oy = std::sin(angle) * dist;
                const float delay = 0.06f + (float(f) / flowers) * D * 0.42f + rnd() * 0.05f;
                const float r = R * (0.2f + rnd() * 0.08f);
                const QRgb color = c(f);
                for (int i = 0; i < per; ++i) {
                    sphere(i, per, 0.3f, dx, dy, dz);
                    const float sp = r * k * (0.92f + rnd() * 0.16f);
                    S s;
                    s.x0 = ox, s.y0 = oy, s.start = delay, s.vx = dx * sp, s.vy = dy * sp, s.drag = k;
                    s.fall = R * 0.16f, s.life = D * (0.32f + rnd() * 0.12f), s.size = R * 0.017f, s.color = color;
                    s.endColor = mixRgb(color, White, 0.6f), s.shift = 0.65f, s.trail = trailK * 0.08f;
                    s.segments = 1, s.fadeTail = 0.25f, s.twinkle = 0.3f + twinkleK * 0.7f, s.depth = depthOf(dz);
                    add(s);
                }
                flash(ox, oy, delay, r * 1.4f, color, 0.22f);
            }
        } else if (kind == u"ring") {
            const float k = 3.4f;
            const int outer = int(std::lround(n * 0.7)), inner = int(std::lround(n * 0.2)), core = n - outer - inner;
            auto ringOf = [&](int count, float reach, QRgb color, float size, float lifeLow) {
                for (int i = 0; i < count; ++i) {
                    planar(std::cos(Tau * i / count), std::sin(Tau * i / count), dx, dy);
                    const float sp = R * k * reach * (0.99f + rnd() * 0.02f);
                    S s;
                    s.vx = dx * sp, s.vy = dy * sp, s.drag = k, s.fall = R * 0.18f, s.life = life(lifeLow, lifeLow + 0.12f);
                    s.size = size, s.color = color, s.endColor = mixRgb(color, White, 0.5f), s.shift = 0.75f;
                    s.trail = (0.05f + trailK * 0.2f) * 0.6f, s.segments = 2, s.twinkle = twinkleK;
                    add(s);
                }
            };
            ringOf(outer, 1, c0, R * 0.022f, 0.85f);
            ringOf(inner, 0.58f, c1, R * 0.018f, 0.7f);
            for (int i = 0; i < core; ++i) {
                sphere(i, std::max(1, core), 0.25f, dx, dy, dz);
                const float sp = R * k * 0.18f;
                S s;
                s.vx = dx * sp, s.vy = dy * sp, s.drag = k, s.fall = R * 0.15f, s.life = life(0.45f, 0.6f);
                s.size = R * 0.02f, s.color = c2, s.depth = depthOf(dz);
                add(s);
            }
            flash(0, 0, 0, R * 0.9f, c0, 0.3f);
        } else if (kind == u"heart" || kind == u"star") {
            const float k = 3.4f;
            const int outline = std::max(12, int(std::lround(n * 0.88)));
            for (int i = 0; i < outline; ++i) {
                const float u = float(i) / outline;
                float px, py;
                if (kind == u"heart") {
                    const float t = u * Tau;
                    px = 16 * std::pow(std::sin(t), 3) / 17;
                    py = -(13 * std::cos(t) - 5 * std::cos(2 * t) - 2 * std::cos(3 * t) - std::cos(4 * t)) / 17;
                } else {
                    const float edge = u * 10;
                    const int e = int(edge);
                    const float f = edge - e;
                    auto vertex = [](int index, float &vx, float &vy) {
                        const float radius = index % 2 == 0 ? 1 : 0.4f;
                        const float a = -float(M_PI) / 2 + index * float(M_PI) / 5;
                        vx = std::cos(a) * radius;
                        vy = std::sin(a) * radius;
                    };
                    float ax, ay, bx, by;
                    vertex(e, ax, ay);
                    vertex(e + 1, bx, by);
                    px = ax + (bx - ax) * f;
                    py = ay + (by - ay) * f;
                }
                planar(px, py, dx, dy);
                const float sp = R * k * (0.99f + rnd() * 0.02f);
                S s;
                s.vx = dx * sp, s.vy = dy * sp, s.drag = k, s.fall = R * 0.1f, s.life = life(0.8f, 0.92f);
                s.size = R * 0.024f, s.color = c0, s.endColor = mixRgb(c0, White, 0.55f), s.shift = 0.7f;
                s.trail = (0.04f + trailK * 0.1f) * 0.6f, s.segments = 1, s.fadeTail = 0.25f, s.twinkle = twinkleK;
                add(s);
            }
            flash(0, 0, 0, R * 0.8f, c0, 0.28f);
        } else if (kind == u"palm") {
            const int branches = std::clamp(int(std::lround(n / 9.0)), 6, 14);
            const int crumbs = std::clamp((n - branches) / branches, 0, 6);
            const float k = 2.3f;
            const QRgb gold = mixRgb(c0, Gold, 0.45f);
            for (int b = 0; b < branches; ++b) {
                const float angle = Tau * b / branches + (rnd() - 0.5f) * 0.35f;
                const float reach = 1 - std::sin(angle) * 0.15f + (rnd() - 0.5f) * 0.1f;
                const float bx = std::cos(angle), by = std::sin(angle);
                const float sp = R * k * reach;
                S s;
                s.vx = bx * sp, s.vy = by * sp, s.drag = k, s.fall = R * 0.7f, s.life = life(0.88f, 0.98f);
                s.size = R * 0.05f, s.color = gold, s.endColor = mixRgb(gold, Ember, 0.55f), s.shift = 0.55f;
                s.trail = (0.4f + trailK * 0.55f) * 0.6f, s.segments = 5, s.fadeTail = 0.4f, s.twinkle = 0.3f + twinkleK * 0.4f;
                add(s);
                for (int j = 0; j < crumbs; ++j) {
                    const float f = 0.9f - j * 0.1f;
                    S c;
                    c.vx = bx * sp * f + (rnd() - 0.5f) * R * 0.3f, c.vy = by * sp * f + (rnd() - 0.5f) * R * 0.3f;
                    c.drag = k, c.fall = R * 0.75f, c.life = life(0.55f, 0.85f), c.size = R * 0.014f;
                    c.color = mixRgb(gold, White, 0.3f), c.trail = 0.05f, c.segments = 1, c.fadeTail = 0.5f, c.twinkle = 0.8f;
                    add(c);
                }
            }
            flash(0, 0, 0, R, gold, 0.4f);
        } else if (kind == u"strobe") {
            const float k = 3.2f;
            for (int i = 0; i < n; ++i) {
                sphere(i, n, 0.3f, dx, dy, dz);
                const float sp = R * k * (0.9f + rnd() * 0.2f);
                S s;
                s.vx = dx * sp, s.vy = dy * sp, s.drag = k, s.fall = R * 0.25f, s.life = life(0.85f, 0.99f);
                s.size = R * 0.022f, s.color = mixRgb(c0, White, 0.5f), s.fadeTail = 0.2f;
                s.strobe = 7 + rnd() * 6, s.depth = depthOf(dz);
                add(s);
            }
            flash(0, 0, 0, R * 0.9f, c0, 0.25f);
        } else { // kiku (default)
            const int core = int(std::lround(n * 0.2)), main = n - core;
            const float k = 3.2f;
            for (int i = 0; i < main; ++i) {
                sphere(i, main, 0.25f, dx, dy, dz);
                const float sp = R * k * (0.94f + rnd() * 0.12f);
                S s;
                s.vx = dx * sp, s.vy = dy * sp, s.drag = k, s.fall = R * 0.3f, s.life = life(0.78f, 0.98f);
                s.size = R * 0.02f, s.color = c0, s.endColor = mixRgb(c0, White, 0.65f), s.shift = 0.7f;
                s.trail = (0.1f + trailK * 0.38f) * 0.7f, s.segments = 3, s.twinkle = twinkleK, s.depth = depthOf(dz);
                add(s);
            }
            for (int i = 0; i < core; ++i) {
                sphere(i, core, 0.25f, dx, dy, dz);
                const float sp = R * 3.6f * 0.42f * (0.95f + rnd() * 0.1f);
                S s;
                s.vx = dx * sp, s.vy = dy * sp, s.drag = 3.6f, s.fall = R * 0.2f, s.life = life(0.4f, 0.55f);
                s.size = R * 0.017f, s.color = c1, s.trail = 0.04f + trailK * 0.06f, s.segments = 1;
                s.fadeTail = 0.2f, s.depth = depthOf(dz);
                add(s);
            }
            flash(0, 0, 0, R * 1.1f, c0, 0.36f);
        }
    }
};

QRgb rgbOf(const QVariant &v)
{
    const QColor c = v.value<QColor>().isValid() ? v.value<QColor>() : QColor(v.toString());
    return c.isValid() ? c.rgb() : White;
}

void starPosition(const SparkField::Star &s, float tau, float &x, float &y)
{
    const float e = std::exp(-s.drag * tau);
    const float f = (1 - e) / s.drag;
    x = s.x0 + s.vx * f;
    y = s.y0 + s.vy * f + s.fall * (tau - f);
}

} // namespace

SparkField::SparkField(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

void SparkField::setTime(double t)
{
    if (t == m_time)
        return;
    // A big jump backwards (seek) keeps events: they are drawn from the time alone.
    m_time = t;
    prune();
    emit timeChanged();
    // Nothing on screen and nothing to draw: skip the scene-graph sync and the upload.
    if (m_events.empty() && m_embers <= 0 && m_lastCount == 0)
        return;
    update();
}

void SparkField::setAmount(double a)
{
    a = std::clamp(a, 0.0, 3.0);
    if (qFuzzyCompare(a, m_amount))
        return;
    m_amount = a;
    emit amountChanged();
}

void SparkField::setEmbers(int n)
{
    n = std::clamp(n, 0, 600);
    if (n == m_embers)
        return;
    m_embers = n;
    emit embersChanged();
    update();
}

void SparkField::setEmberColor(const QColor &c)
{
    if (c == m_emberColor)
        return;
    m_emberColor = c;
    emit embersChanged();
}

void SparkField::add(Event &&e)
{
    m_events.push_back(std::move(e));
    // Bounded: drop the oldest events beyond a generous limit.
    if (m_events.size() > 400)
        m_events.erase(m_events.begin(), m_events.begin() + long(m_events.size() - 400));
    update();
}

void SparkField::prune()
{
    // Keep events for a while after they end, so a short seek backwards still finds them.
    const double keepUntil = m_time - 20.0;
    m_events.erase(std::remove_if(m_events.begin(), m_events.end(),
                                  [keepUntil](const Event &e) { return e.end < keepUntil; }),
                   m_events.end());
}

void SparkField::burst(const QString &kind, double x, double y, double at, const QVariantList &colors, double radius,
                       double duration, int count, double trail, double twinkle, double squash, double rotation,
                       int seed, int group)
{
    Event e;
    e.group = group;
    e.start = at;
    e.end = at + std::max(0.2, duration);
    std::mt19937 rng(quint32(seed) * 2654435761U ^ quint32(qHash(kind)));
    const float tilt = std::uniform_real_distribution<float>(0, Tau)(rng);
    Planner p{e,
              rng,
              float(x),
              float(y),
              float(at),
              float(std::max(4.0, radius)),
              float(std::max(0.2, duration)),
              std::max(0, int(std::lround(count * m_amount))),
              float(std::clamp(trail, 0.0, 1.0)),
              float(std::clamp(twinkle, 0.0, 1.0)),
              float(squash),
              float(rotation),
              {},
              std::cos(tilt),
              std::sin(tilt),
              std::max(1.4f, float(radius) * 0.006f)};
    for (const QVariant &c : colors)
        p.colors.push_back(rgbOf(c));
    p.plan(kind);
    add(std::move(e));
}

void SparkField::launch(double x0, double y0, double x1, double y1, double at, double duration, const QColor &color,
                        int seed, int group)
{
    Event e;
    e.group = group;
    e.start = at;
    e.end = at + duration + 0.3;
    e.launches.push_back({float(x0), float(y0), float(x1), float(y1), float(at), float(std::max(0.1, duration)),
                          color.rgb(), float(hash01(quint32(seed), 7) * Tau)});
    add(std::move(e));
}

void SparkField::sparkle(double x, double y, double at, const QColor &color, double radius, int count, int seed,
                         int group)
{
    Event e;
    e.group = group;
    e.start = at;
    e.end = at + 0.9;
    std::mt19937 rng(quint32(seed) * 747796405U + 2891336453U);
    std::uniform_real_distribution<float> u(0, 1);
    const int n = std::max(1, int(std::lround(count * m_amount)));
    for (int i = 0; i < n; ++i) {
        const float a = u(rng) * Tau;
        const float sp = float(radius) * (2.0f + u(rng) * 2.5f);
        e.stars.push_back({float(x), float(y), std::cos(a) * sp, std::sin(a) * sp - float(radius) * 0.8f, 3.5f,
                           float(radius) * 0.9f, float(at) + u(rng) * 0.08f, 0.45f + u(rng) * 0.35f,
                           std::max(1.4f, float(radius) * 0.09f), color.rgb(), mixRgb(color.rgb(), White, 0.7f), 0.5f,
                           0.05f, 1, 0.4f, 0, 0.6f, 1, quint32(rng())});
    }
    add(std::move(e));
}

void SparkField::removeGroup(int group)
{
    m_events.erase(std::remove_if(m_events.begin(), m_events.end(), [group](const Event &e) { return e.group == group; }),
                   m_events.end());
    update();
}

void SparkField::clear()
{
    m_events.clear();
    update();
}

QSGNode *SparkField::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    auto *node = static_cast<QSGGeometryNode *>(old);
    if (!node) {
        node = new QSGGeometryNode;
        auto *geometry = new QSGGeometry(attributes(), 0);
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);
        geometry->setVertexDataPattern(QSGGeometry::StreamPattern);
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(new SparkMaterial);
        node->setFlag(QSGNode::OwnsMaterial);
    }

    // Reused between frames (one render thread): no allocation per frame once warm.
    static thread_local std::vector<Vertex> verts;
    verts.clear();
    QuadWriter w{verts};
    const float t = float(m_time);

    // Embers: closed form from the time, rising and swaying.
    if (m_embers > 0) {
        const float W = float(width()), H = float(height());
        const QRgb ec = m_emberColor.rgb();
        for (int i = 0; i < m_embers && !w.full(); ++i) {
            const float h1 = hash01(quint32(i), 1), h2 = hash01(quint32(i), 2), h3 = hash01(quint32(i), 3);
            const float h4 = hash01(quint32(i), 4);
            const float speed = H * (0.015f + h3 * 0.035f);
            const float span = H + 60;
            const float y = H + 30 - std::fmod(h4 * span + t * speed, span);
            const float x = h1 * W + std::sin(t * (0.3f + h2 * 0.5f) + h2 * 6.28f) * 24;
            const float flicker = 0.55f + 0.45f * std::sin(t * (1.5f + h2 * 3) + h1 * 20);
            w.dot(x, y, 2.0f + h3 * 4.0f, ec, (0.12f + 0.3f * h2) * flicker);
        }
    }

    int count = 0;
    for (const Event &e : m_events) {
        if (t < e.start - 0.001 || t > e.end)
            continue;
        for (const Launch &l : e.launches) {
            const float tau = t - l.at;
            if (tau < 0 || tau > l.duration + 0.25f)
                continue;
            auto headAt = [&l](float tt, float &hx, float &hy) {
                const float q = std::clamp(tt / l.duration, 0.0f, 1.0f);
                const float eased = 1 - (1 - q) * (1 - q);
                hx = l.x0 + (l.x1 - l.x0) * eased + std::sin(q * 13 + l.phase) * 5 * (1 - q);
                hy = l.y0 + (l.y1 - l.y0) * eased;
            };
            const float fade = tau > l.duration ? 1 - (tau - l.duration) / 0.25f : 1;
            float hx, hy;
            headAt(std::min(tau, l.duration), hx, hy);
            for (int s = 0; s < 5; ++s) {
                const float a0 = std::max(0.0f, tau - 0.06f * (s + 1)), a1 = std::max(0.0f, tau - 0.06f * s);
                float x0, y0, x1, y1;
                headAt(std::min(a0, l.duration), x0, y0);
                headAt(std::min(a1, l.duration), x1, y1);
                w.segment(x0, y0, x1, y1, 2.2f, 1 - (s + 1) / 5.0f, 1 - s / 5.0f, mixRgb(l.color, Ember, 0.3f), 0.8f * fade);
            }
            if (tau <= l.duration)
                w.dot(hx, hy, 5, mixRgb(l.color, White, 0.6f), 0.9f);
        }
        for (const Flash &f : e.flashes) {
            const float tau = t - f.at;
            if (tau < 0 || tau > f.life)
                continue;
            const float p = tau / f.life;
            w.dot(f.x, f.y, f.size * (0.6f + 0.4f * std::sqrt(p)), f.color, (1 - p) * (1 - p) * 0.55f);
        }
        for (const Star &s : e.stars) {
            const float tau = t - s.start;
            if (tau < 0 || tau > s.life || w.full())
                continue;
            const float p = tau / s.life;
            float a = s.depth * std::min(1.0f, tau / 0.03f);
            const float fadeStart = 1 - s.fadeTail;
            if (p > fadeStart)
                a *= std::max(0.0f, 1 - (p - fadeStart) / std::max(0.01f, s.fadeTail));
            if (s.twinkle > 0 && p > 0.45f)
                a *= 1 - s.twinkle * 0.65f * hash01(quint32(t * 24), s.hash);
            if (s.strobe > 0 && p > 0.3f)
                a *= std::fmod(t * s.strobe + float(s.hash % 97) / 97.0f, 1.0f) < 0.5f ? 1.0f : 0.1f;
            if (a <= 0.004f)
                continue;
            const QRgb color = mixRgb(s.color, s.endColor, (p - (s.shift - 0.1f)) / 0.2f);
            float x, y;
            starPosition(s, tau, x, y);
            const float size = s.size * (1 - 0.35f * p);
            for (int j = 0; j < s.segments; ++j) {
                const float t1 = tau - s.trail * j / s.segments;
                if (t1 <= 0)
                    break;
                const float t0 = std::max(0.0f, tau - s.trail * (j + 1) / s.segments);
                float x0, y0, x1, y1;
                starPosition(s, t0, x0, y0);
                starPosition(s, t1, x1, y1);
                w.segment(x0, y0, x1, y1, size * 0.9f, 1 - float(j + 1) / s.segments, 1 - float(j) / s.segments,
                          mixRgb(color, Ember, 0.25f), a * 0.8f);
            }
            w.dot(x, y, size * 2.2f, color, a);
            ++count;
        }
    }
    m_lastCount = count;

    QSGGeometry *g = node->geometry();
    g->allocate(int(verts.size()));
    if (!verts.empty())
        std::memcpy(g->vertexData(), verts.data(), verts.size() * sizeof(Vertex));
    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}
