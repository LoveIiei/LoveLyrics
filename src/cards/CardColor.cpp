// SPDX-License-Identifier: AGPL-3.0-or-later
// Ported from spacedge/lyric-card-generator (AGPL-3.0), src/color.js.
#include "CardColor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace CardColor {

namespace {

using Lab = std::array<double, 3>;

double lin(double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); }
double gam(double c) { return c <= 0.0031308 ? 12.92 * c : 1.055 * std::pow(c, 1 / 2.4) - 0.055; }

const std::array<double, 256> &linTable()
{
    static const std::array<double, 256> t = [] {
        std::array<double, 256> a{};
        for (int i = 0; i < 256; ++i)
            a[size_t(i)] = lin(i / 255.0);
        return a;
    }();
    return t;
}

double toDeg(double rad)
{
    double d = std::fmod(rad * 180 / M_PI, 360.0);
    return d < 0 ? d + 360 : d;
}

double clampd(double v, double lo, double hi) { return std::min(hi, std::max(lo, v)); }

Lab rgbToOklab(int r, int g, int b)
{
    const auto &T = linTable();
    const double lr = T[size_t(r)], lg = T[size_t(g)], lb = T[size_t(b)];
    const double l = std::cbrt(0.4122214708 * lr + 0.5363325363 * lg + 0.0514459929 * lb);
    const double m = std::cbrt(0.2119034982 * lr + 0.6806995451 * lg + 0.1073969566 * lb);
    const double s = std::cbrt(0.0883024619 * lr + 0.2817188376 * lg + 0.6299787005 * lb);
    return {0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
            1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
            0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s};
}

Lab labOf(const QColor &c) { return rgbToOklab(c.red(), c.green(), c.blue()); }

std::array<double, 3> oklabToLinear(double L, double a, double b)
{
    const double l = std::pow(L + 0.3963377774 * a + 0.2158037573 * b, 3);
    const double m = std::pow(L - 0.1055613458 * a - 0.0638541728 * b, 3);
    const double s = std::pow(L - 0.0894841775 * a - 1.2914855480 * b, 3);
    return {4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
            -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
            -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s};
}

std::array<double, 3> okToLinear(double L, double C, double H)
{
    const double rad = H * M_PI / 180;
    return oklabToLinear(L, C * std::cos(rad), C * std::sin(rad));
}

QColor linearToColor(const std::array<double, 3> &rgb)
{
    auto ch = [](double v) { return int(std::lround(gam(clampd(v, 0, 1)) * 255)); };
    return QColor(ch(rgb[0]), ch(rgb[1]), ch(rgb[2]));
}

double Yof(double T) { return T > 8 ? std::pow((T + 16) / 116, 3) : T / 903.3; }

double labDist2(const Lab &p, const Lab &q)
{
    return (p[0] - q[0]) * (p[0] - q[0]) + (p[1] - q[1]) * (p[1] - q[1]) + (p[2] - q[2]) * (p[2] - q[2]);
}
double labDist(const Lab &p, const Lab &q) { return std::sqrt(labDist2(p, q)); }

constexpr int SampleSize = 96;
constexpr int MaxClusters = 12;
constexpr double SeedStopDist = 0.03;
constexpr int KmeansIterations = 10;
constexpr double MergeDist = 0.05;
constexpr double VividChroma = 0.04;
constexpr double VividShare = 0.006;
constexpr double AccentMinDist = 0.15;
constexpr double SwatchMinDist = 0.12;
constexpr int MaxSwatches = 6;
constexpr double NeutralShare = 0.05;
constexpr double NeutralMinDist = 0.25;
constexpr double EdgeFraction = 0.06;

struct Cluster
{
    Lab lab;
    double share = 0;
};

std::vector<Cluster> clusterColours(const std::vector<Lab> &labs)
{
    Lab mean{0, 0, 0};
    for (const auto &p : labs)
        for (int k = 0; k < 3; ++k)
            mean[size_t(k)] += p[size_t(k)];
    for (auto &v : mean)
        v /= double(labs.size());

    std::vector<Lab> centres{mean};
    std::vector<double> nearest(labs.size());
    for (size_t i = 0; i < labs.size(); ++i)
        nearest[i] = labDist2(labs[i], mean);
    while (int(centres.size()) < MaxClusters) {
        size_t far = 0;
        for (size_t i = 1; i < labs.size(); ++i)
            if (nearest[i] > nearest[far])
                far = i;
        if (nearest[far] < SeedStopDist * SeedStopDist)
            break;
        centres.push_back(labs[far]);
        for (size_t i = 0; i < labs.size(); ++i)
            nearest[i] = std::min(nearest[i], labDist2(labs[i], labs[far]));
    }

    std::vector<int> assign(labs.size(), 0);
    for (int iter = 0; iter < KmeansIterations; ++iter) {
        std::vector<std::array<double, 4>> sums(centres.size(), {0, 0, 0, 0});
        for (size_t i = 0; i < labs.size(); ++i) {
            size_t best = 0;
            double bestD = 1e300;
            for (size_t j = 0; j < centres.size(); ++j) {
                const double d = labDist2(labs[i], centres[j]);
                if (d < bestD) {
                    bestD = d;
                    best = j;
                }
            }
            assign[i] = int(best);
            auto &s = sums[best];
            s[0] += labs[i][0];
            s[1] += labs[i][1];
            s[2] += labs[i][2];
            s[3] += 1;
        }
        for (size_t j = 0; j < centres.size(); ++j)
            if (sums[j][3] > 0)
                centres[j] = {sums[j][0] / sums[j][3], sums[j][1] / sums[j][3], sums[j][2] / sums[j][3]};
    }

    struct Alive
    {
        Lab lab;
        double n;
    };
    std::vector<Alive> alive;
    std::vector<double> counts(centres.size(), 0);
    for (int a : assign)
        counts[size_t(a)] += 1;
    for (size_t j = 0; j < centres.size(); ++j)
        if (counts[j] > 0)
            alive.push_back({centres[j], counts[j]});

    // Merge clusters closer than MergeDist (area-weighted), restarting after each merge.
    for (bool merged = true; merged;) {
        merged = false;
        for (size_t i = 0; i < alive.size() && !merged; ++i)
            for (size_t j = i + 1; j < alive.size() && !merged; ++j)
                if (labDist(alive[i].lab, alive[j].lab) < MergeDist) {
                    const double n = alive[i].n + alive[j].n;
                    for (int k = 0; k < 3; ++k)
                        alive[i].lab[size_t(k)] =
                            (alive[i].lab[size_t(k)] * alive[i].n + alive[j].lab[size_t(k)] * alive[j].n) / n;
                    alive[i].n = n;
                    alive.erase(alive.begin() + long(j));
                    merged = true;
                }
    }
    std::vector<Cluster> out;
    for (const auto &a : alive)
        out.push_back({a.lab, a.n / double(labs.size())});
    std::stable_sort(out.begin(), out.end(), [](const Cluster &p, const Cluster &q) { return p.share > q.share; });
    return out;
}

QPointF visualFocus(const std::vector<std::optional<Lab>> &labs, int size, double meanL)
{
    double meanA = 0, meanB = 0;
    int n = 0;
    for (const auto &p : labs)
        if (p) {
            meanA += (*p)[1];
            meanB += (*p)[2];
            ++n;
        }
    meanA /= std::max(1, n);
    meanB /= std::max(1, n);

    std::vector<double> weights(labs.size(), 0);
    std::vector<double> valid;
    for (int i = 0; i < int(labs.size()); ++i) {
        if (!labs[size_t(i)])
            continue;
        const Lab &lab = *labs[size_t(i)];
        const int x = i % size, y = i / size;
        double sum = 0;
        int count = 0;
        for (int yy = std::max(0, y - 3); yy <= std::min(size - 1, y + 3); ++yy)
            for (int xx = std::max(0, x - 3); xx <= std::min(size - 1, x + 3); ++xx)
                if (const auto &near = labs[size_t(yy * size + xx)]) {
                    sum += (*near)[0];
                    ++count;
                }
        const double fx = (x + 0.5) / size, fy = (y + 0.5) / size;
        const double border = std::min({fx, 1 - fx, fy, 1 - fy}) < EdgeFraction ? 0.3 : 1;
        weights[size_t(i)] = border * (2 * std::abs(lab[0] - sum / count) + std::hypot(lab[1] - meanA, lab[2] - meanB)
                                       + 0.6 * std::abs(lab[0] - meanL));
        valid.push_back(weights[size_t(i)]);
    }
    if (valid.empty())
        return {0.5, 0.5};
    std::sort(valid.begin(), valid.end(), std::greater<>());
    const double cut = valid[size_t(std::floor(valid.size() * 0.1))];
    double wx = 0, wy = 0, total = 0;
    for (int i = 0; i < int(labs.size()); ++i) {
        if (!labs[size_t(i)] || weights[size_t(i)] < cut)
            continue;
        wx += weights[size_t(i)] * ((i % size) + 0.5) / size;
        wy += weights[size_t(i)] * (double(i / size) + 0.5) / size; // row index
        total += weights[size_t(i)];
    }
    return total > 1e-9 ? QPointF(wx / total, wy / total) : QPointF(0.5, 0.5);
}

QColor meanRows(const QImage &img, int row0, int row1, const QColor &fallback)
{
    double r = 0, g = 0, b = 0;
    int n = 0;
    for (int y = row0; y < row1; ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            if (qAlpha(line[x]) < 200)
                continue;
            r += qRed(line[x]);
            g += qGreen(line[x]);
            b += qBlue(line[x]);
            ++n;
        }
    }
    return n ? QColor(int(std::lround(r / n)), int(std::lround(g / n)), int(std::lround(b / n))) : fallback;
}

// ---- card background (four styles) ----------------------------------------------------

constexpr double StyleGap = 0.08;
constexpr int VibrantLo = 26, VibrantHi = 76;

double gapTo(const QColor &c, const QList<QColor> &others)
{
    double m = 1e300;
    for (const auto &o : others)
        m = std::min(m, okDistance(c, o));
    return m;
}

QColor firstSeparated(const QList<QColor> &candidates, const QList<QColor> &others)
{
    QColor best = candidates.first();
    double bestGap = -1e300;
    for (const auto &c : candidates) {
        const double gap = gapTo(c, others);
        if (gap >= StyleGap)
            return c;
        if (gap > bestGap) {
            best = c;
            bestGap = gap;
        }
    }
    return best;
}

int vibrantStartTone(Palette &pal, bool dark)
{
    auto chromaAt = [&](int t) { return toOklch(pal(t)).C; };
    int peakTone = 50;
    double peakChroma = 0;
    for (int t = VibrantLo; t <= VibrantHi; t += 2) {
        const double c = chromaAt(t);
        if (c > peakChroma) {
            peakChroma = c;
            peakTone = t;
        }
    }
    const int step = dark ? -2 : 2;
    int tone = peakTone;
    while (tone + step >= VibrantLo && tone + step <= VibrantHi && chromaAt(tone + step) >= peakChroma * 0.85)
        tone += step;
    return tone;
}

QColor cardBackground(const QColor &seed, const QColor &accent, Style style, bool dark, bool forced)
{
    const Oklch s = toOklch(seed);
    const double T0 = toneOf(seed);
    const QColor seedAdj = s.C > 0.22 ? Palette(s.H, 0.2)(T0) : seed;

    const QColor content = !forced || (dark ? T0 <= 32 : T0 >= 72) ? seedAdj
                                                                    : Palette(s.H, std::min(s.C, 0.2))(dark ? 26 : 76);
    const QColor mono = Palette(0, 0)(dark ? 8 : 98);
    if (style == Style::Mono)
        return mono;
    if (style == Style::Content)
        return content;

    Palette tonalPal(s.H, clampd(s.C * 0.45, 0.02, 0.05));
    QList<QColor> tonalCandidates;
    for (int t : dark ? std::array<int, 5>{18, 22, 14, 26, 36} : std::array<int, 5>{88, 84, 92, 80, 74})
        tonalCandidates << tonalPal(t);
    const QColor tonal = firstSeparated(tonalCandidates, {content, mono});
    if (style == Style::Tonal)
        return tonal;

    const Oklch src = s.C < VividChroma && accent.isValid() ? toOklch(accent) : s;
    if (src.C < VividChroma)
        return firstSeparated(dark ? QList<QColor>{Qt::black, Qt::white} : QList<QColor>{Qt::white, Qt::black},
                              {content, tonal, mono});
    Palette pal(src.H, 0.4);
    const int start = vibrantStartTone(pal, dark);
    const int away = start >= toneOf(content) ? 2 : -2;
    QList<QColor> tones;
    for (int t = start; t >= VibrantLo && t <= VibrantHi; t += away)
        tones << pal(t);
    for (int t = start - away; t >= VibrantLo && t <= VibrantHi; t -= away)
        tones << pal(t);
    return firstSeparated(tones, {content, tonal, mono});
}

} // namespace

Oklch toOklch(const QColor &c)
{
    const Lab lab = labOf(c);
    return {lab[0], std::hypot(lab[1], lab[2]), toDeg(std::atan2(lab[2], lab[1]))};
}

QColor fromHex(const QString &hex)
{
    QColor c(hex);
    return c.isValid() ? c : QColor(FallbackSeed);
}

QString toHex(const QColor &c)
{
    return c.name(QColor::HexRgb).toUpper();
}

QColor toneColor(double H, double C, double T)
{
    const double L = std::cbrt(Yof(T));
    double lo = 0, hi = C;
    for (int i = 0; i < 30; ++i) {
        const double mid = (lo + hi) / 2;
        const auto rgb = okToLinear(L, mid, H);
        if (std::all_of(rgb.begin(), rgb.end(), [](double v) { return v >= -1e-4 && v <= 1 + 1e-4; }))
            lo = mid;
        else
            hi = mid;
    }
    return linearToColor(okToLinear(L, lo, H));
}

QColor Palette::operator()(double tone)
{
    const int key = int(std::lround(tone * 100));
    auto it = m_cache.find(key);
    if (it != m_cache.end())
        return it->second;
    const QColor c = toneColor(m_H, m_C, tone);
    m_cache.emplace(key, c);
    return c;
}

double luminance(const QColor &c)
{
    const auto &T = linTable();
    return 0.2126 * T[size_t(c.red())] + 0.7152 * T[size_t(c.green())] + 0.0722 * T[size_t(c.blue())];
}

double contrast(const QColor &a, const QColor &b)
{
    const double la = luminance(a), lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

double toneOf(const QColor &c)
{
    const double Y = luminance(c);
    return Y > 0.008856 ? 116 * std::cbrt(Y) - 16 : 903.3 * Y;
}

double hueDistance(double h1, double h2)
{
    const double d = std::fmod(std::abs(h1 - h2), 360.0);
    return d > 180 ? 360 - d : d;
}

QColor ensureContrast(Palette &pal, double tone, const QColor &bg, double min, int dir)
{
    const double step = dir < 0 ? -2 : 2;
    double t = clampd(tone, 0, 100);
    QColor c = pal(t);
    while (contrast(c, bg) < min && t > 0 && t < 100) {
        t = clampd(t + step, 0, 100);
        c = pal(t);
    }
    return c;
}

double okDistance(const QColor &a, const QColor &b)
{
    return labDist(labOf(a), labOf(b));
}

QColor withAlpha(QColor c, double a)
{
    c.setAlphaF(float(std::clamp(a, 0.0, 1.0)));
    return c;
}

CoverAnalysis fallbackAnalysis()
{
    const QColor seed(FallbackSeed);
    const Oklch o = toOklch(seed);
    CoverAnalysis a;
    a.swatches = {{seed, o.L, o.C, o.H, 1.0, QStringLiteral("base")}};
    a.edgeTop = a.edgeBottom = seed;
    return a;
}

CoverAnalysis analyzeCover(const QImage &cover)
{
    if (cover.isNull())
        return fallbackAnalysis();
    // object-fit: cover into a 96 px square.
    const QImage square = [&] {
        const QImage big = cover.convertToFormat(QImage::Format_ARGB32)
                               .scaled(SampleSize, SampleSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        return big.copy((big.width() - SampleSize) / 2, (big.height() - SampleSize) / 2, SampleSize, SampleSize);
    }();

    std::vector<std::optional<Lab>> labs(size_t(SampleSize * SampleSize));
    std::vector<Lab> opaque;
    for (int y = 0; y < SampleSize; ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(square.constScanLine(y));
        for (int x = 0; x < SampleSize; ++x) {
            if (qAlpha(line[x]) < 200)
                continue;
            const Lab lab = rgbToOklab(qRed(line[x]), qGreen(line[x]), qBlue(line[x]));
            labs[size_t(y * SampleSize + x)] = lab;
            opaque.push_back(lab);
        }
    }
    if (opaque.empty())
        return fallbackAnalysis();

    struct C
    {
        Lab lab;
        double share;
        QColor color;
        double L, Cc, H;
    };
    std::vector<C> clusters;
    for (const auto &cl : clusterColours(opaque))
        clusters.push_back({cl.lab, cl.share, linearToColor(oklabToLinear(cl.lab[0], cl.lab[1], cl.lab[2])), cl.lab[0],
                            std::hypot(cl.lab[1], cl.lab[2]), toDeg(std::atan2(cl.lab[2], cl.lab[1]))});
    const C &base = clusters.front();
    auto score = [](const C &c) { return std::sqrt(c.share) * c.Cc * 100; };
    std::vector<const C *> vivid;
    for (const auto &c : clusters)
        if (c.Cc >= VividChroma && c.share >= VividShare)
            vivid.push_back(&c);
    std::stable_sort(vivid.begin(), vivid.end(), [&](const C *p, const C *q) { return score(*p) > score(*q); });
    const C *accent = nullptr;
    for (const C *c : vivid)
        if (labDist(c->lab, base.lab) >= AccentMinDist) {
            accent = c;
            break;
        }

    std::vector<std::pair<const C *, QString>> picked{{&base, QStringLiteral("base")}};
    auto add = [&](const C *c, const QString &role) {
        if (int(picked.size()) >= MaxSwatches)
            return;
        for (const auto &p : picked)
            if (labDist(p.first->lab, c->lab) < SwatchMinDist)
                return;
        picked.push_back({c, role});
    };
    for (const C *c : vivid)
        add(c, c == accent ? QStringLiteral("accent") : QStringLiteral("vivid"));
    for (const auto &c : clusters)
        if (c.Cc < VividChroma && c.share >= NeutralShare && labDist(c.lab, base.lab) >= NeutralMinDist)
            add(&c, QStringLiteral("neutral"));

    CoverAnalysis a;
    for (const auto &[c, role] : picked)
        a.swatches.append({c->color, c->L, c->Cc, c->H, c->share, role});
    double vividShare = 0;
    for (const C *c : vivid)
        vividShare += c->share;
    const bool isGround = (base.L < 0.25 || base.L > 0.92) && base.share >= 0.25;
    a.defaultIndex = base.Cc < VividChroma && !isGround && vividShare >= 0.2 && a.swatches.size() > 1 ? 1 : 0;
    a.accent = accent ? accent->color : QColor();
    double sumL = 0;
    for (const auto &p : opaque)
        sumL += p[0];
    a.meanL = sumL / double(opaque.size());
    a.focus = visualFocus(labs, SampleSize, a.meanL);
    const int edgeRows = std::max(1, int(std::lround(SampleSize * EdgeFraction)));
    a.edgeTop = meanRows(square, 0, edgeRows, base.color);
    a.edgeBottom = meanRows(square, SampleSize - edgeRows, SampleSize, base.color);
    return a;
}

bool resolveDark(Mode mode, double meanL)
{
    if (mode == Mode::Dark)
        return true;
    if (mode == Mode::Light)
        return false;
    return meanL < 0.5;
}

CardPalette cardPalette(const QColor &seed, const QColor &accent, Style style, Mode mode, double meanL)
{
    const bool dark = resolveDark(mode, meanL);
    const QColor bg = cardBackground(seed, accent, style, dark, mode != Mode::Auto);
    const Oklch b = toOklch(bg);
    const bool isDark = contrast(bg, Qt::white) >= contrast(bg, Qt::black);
    const int dir = isDark ? 1 : -1;

    Palette onPal(b.H, std::min(b.C * 0.25, 0.025));
    Palette onVarPal(b.H, std::min(std::max(b.C * 0.5, 0.02), 0.08));
    Palette containerPal(b.H, b.C * 0.9);

    CardPalette p;
    p.bg = bg;
    p.isDark = isDark;
    p.on = ensureContrast(onPal, isDark ? 97 : 12, bg, 7, dir);
    p.onVar = ensureContrast(onVarPal, isDark ? 80 : 35, bg, 4.5, dir);
    p.container = containerPal(clampd(toneOf(bg) + (isDark ? 8 : -6), 4, 98));

    Oklch a;
    bool haveAccent = accent.isValid();
    if (haveAccent)
        a = toOklch(accent);
    if (!haveAccent || (b.C >= VividChroma && hueDistance(a.H, b.H) < 40))
        a = {0, clampd(b.C * 0.6, 0.06, 0.12), std::fmod(b.H + 175, 360.0)};
    Palette accentPal(a.H, std::max(a.C, 0.06));
    p.accent = ensureContrast(accentPal, isDark ? 86 : 38, bg, 4.5, dir);
    p.track = withAlpha(p.on, 0.16);
    p.shadow = isDark ? QColor(0, 0, 0, 128) : withAlpha(p.on, 0.25);
    return p;
}

} // namespace CardColor
