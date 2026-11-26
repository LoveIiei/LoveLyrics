// SPDX-License-Identifier: AGPL-3.0-or-later
// Fireworks layer for the PV styles. Each burst is expanded once into "stars" whose position,
// brightness and colour at any time come from a closed-form solution (velocity, linear drag,
// terminal fall speed), so the picture depends only on `time`: seeking and pausing are exact.
// All sparks, trail segments, flashes and launch streaks are one geometry node (one draw call)
// with an additive glow shader.
//
// The burst model and the firework kinds follow Folia's Hanabi mode (AGPL-3.0,
// chthollyphile/folia-major): src/components/visualizer/hanabi/mg/burstPlan.ts.
#pragma once

#include <QColor>
#include <QQuickItem>
#include <QVariantList>

#include <vector>

class SparkField : public QQuickItem
{
    Q_OBJECT
    // Scene clock, seconds (usually the lyrics position / 1000).
    Q_PROPERTY(double time READ time WRITE setTime NOTIFY timeChanged)
    // Multiplies spark counts (quality / battery).
    Q_PROPERTY(double amount READ amount WRITE setAmount NOTIFY amountChanged)
    // Slowly rising embers over the whole item (0 = none).
    Q_PROPERTY(int embers READ embers WRITE setEmbers NOTIFY embersChanged)
    Q_PROPERTY(QColor emberColor READ emberColor WRITE setEmberColor NOTIFY embersChanged)
    Q_PROPERTY(int sparkCount READ sparkCount NOTIFY timeChanged) // drawn last frame (debug)

public:
    explicit SparkField(QQuickItem *parent = nullptr);

    double time() const { return m_time; }
    void setTime(double t);
    double amount() const { return m_amount; }
    void setAmount(double a);
    int embers() const { return m_embers; }
    void setEmbers(int n);
    QColor emberColor() const { return m_emberColor; }
    void setEmberColor(const QColor &c);
    int sparkCount() const { return m_lastCount; }

    // A firework of `kind` (kiku, botan, yanagi, kamuro, senrin, ring, heart, star, palm,
    // strobe) at (x, y) item px, starting at `at` seconds. radius in px; colors: list of
    // colours (cycled); group: tag for removeGroup().
    Q_INVOKABLE void burst(const QString &kind, double x, double y, double at, const QVariantList &colors,
                           double radius, double duration, int count = 90, double trail = 0.5,
                           double twinkle = 0.3, double squash = 1.0, double rotation = 0.0, int seed = 0,
                           int group = -1);
    // A rising launch trail from (x0, y0) to (x1, y1) during [at, at + duration].
    Q_INVOKABLE void launch(double x0, double y0, double x1, double y1, double at, double duration,
                            const QColor &color, int seed = 0, int group = -1);
    // A small spray of glitter (word landed, glyph dissolving...).
    Q_INVOKABLE void sparkle(double x, double y, double at, const QColor &color, double radius, int count = 12,
                             int seed = 0, int group = -1);
    Q_INVOKABLE void removeGroup(int group);
    Q_INVOKABLE void clear();

    struct Star
    {
        float x0, y0, vx, vy, drag, fall;
        float start, life, size;
        QRgb color, endColor;
        float shift;
        float trail;
        int segments;
        float fadeTail;
        float strobe;
        float twinkle;
        float depth;
        quint32 hash;
    };
    struct Flash
    {
        float x, y, at, size, life;
        QRgb color;
    };
    struct Launch
    {
        float x0, y0, x1, y1, at, duration;
        QRgb color;
        float phase;
    };
    struct Event
    {
        int group = -1;
        double start = 0, end = 0;
        std::vector<Star> stars;
        std::vector<Flash> flashes;
        std::vector<Launch> launches;
    };

signals:
    void timeChanged();
    void amountChanged();
    void embersChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;

private:
    void add(Event &&e);
    void prune();

    double m_time = 0;
    double m_amount = 1.0;
    int m_embers = 0;
    QColor m_emberColor = QColor(255, 200, 140);
    std::vector<Event> m_events;
    int m_lastCount = 0;
};
