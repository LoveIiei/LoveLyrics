// SPDX-License-Identifier: AGPL-3.0-or-later
// Chorus detection and section rules follow Folia (AGPL-3.0, chthollyphile/folia-major):
// src/utils/chorusDetector.ts and src/components/visualizer/paragraphStructure.ts.
#include "SongStructure.h"

#include <QHash>
#include <QRegularExpression>
#include <unicode/brkiter.h>
#include <unicode/locid.h>
#include <unicode/unistr.h>

#include <algorithm>
#include <limits>
#include <memory>

namespace SongStructure {

namespace {

constexpr double GapMultiplier = 2.5;
constexpr qint64 GapMinMs = 1250, GapMaxMs = 3500;
constexpr int MaxLines = 6;
constexpr qint64 MaxDurationMs = 18000;

QString normalized(const QString &text)
{
    static const QRegularExpression punct(QStringLiteral(R"([\p{P}\p{S}\s]+)"));
    return text.toLower().remove(punct);
}

quint32 fnv1a(const QString &s, quint32 h = 2166136261u)
{
    const QByteArray bytes = s.toUtf8();
    for (char c : bytes) {
        h ^= quint8(c);
        h *= 16777619u;
    }
    return h;
}

bool isWordLike(const QString &segment)
{
    for (const QChar c : segment)
        if (c.isLetterOrNumber())
            return true;
    return false;
}

// Visual end of a line: its end, but never past the next line's start.
qint64 renderEnd(const QList<LyricLine> &lines, int i)
{
    const qint64 next = i + 1 < lines.size() ? lines[i + 1].startMs : std::numeric_limits<qint64>::max();
    return std::max(lines[i].startMs, std::min(lines[i].endMs, next));
}

// Oversized sections split at their largest gap (left halves keep splitting too).
void splitOversized(const QList<LyricLine> &lines, QList<int> section, QList<QList<int>> &out)
{
    for (int guard = 0; guard < 1000; ++guard) {
        const qsizetype n = section.size();
        const bool tooLong = n > 1 && renderEnd(lines, section.last()) - lines[section.first()].startMs > MaxDurationMs;
        if (n <= MaxLines && !tooLong)
            break;
        qsizetype split = std::min<qsizetype>(4, n - 1);
        qint64 bestGap = -1;
        for (qsizetype k = 2; k < n - 1; ++k) {
            const qint64 gap = lines[section[k]].startMs - renderEnd(lines, section[k - 1]);
            if (gap > bestGap) {
                bestGap = gap;
                split = k;
            }
        }
        split = std::max<qsizetype>(1, split);
        splitOversized(lines, section.mid(0, split), out);
        section = section.mid(split);
    }
    if (!section.isEmpty())
        out << section;
}

QString classify(const QList<LyricLine> &lines, const QList<int> &section, bool last)
{
    for (int i : section)
        if (lines[i].isChorus)
            return QStringLiteral("chorus");
    if (last)
        return QStringLiteral("outro");
    const qint64 duration = renderEnd(lines, section.last()) - lines[section.first()].startMs;
    int words = 0, punctuation = 0;
    for (int i : section) {
        words += int(units(lines[i]).size());
        for (const QChar c : lines[i].text)
            if (QStringLiteral("!?！？…").contains(c))
                ++punctuation;
    }
    if (duration <= 3500 || words <= 3)
        return QStringLiteral("breath");
    if (punctuation >= 2 || words / std::max(1.0, duration / 1000.0) > 2.5)
        return QStringLiteral("lift");
    return QStringLiteral("verse");
}

} // namespace

void analyze(LyricsDocument &doc)
{
    auto &lines = doc.lines;
    doc.sections.clear();

    // Seed: the same song gives the same choices every time.
    quint32 h = fnv1a(doc.title);
    h = fnv1a(doc.artist, h);
    for (const auto &l : std::as_const(lines))
        h = fnv1a(l.text, h);
    doc.seed = h;

    // Chorus: the most repeated line text (if anything repeats).
    QHash<QString, int> counts;
    for (const auto &l : std::as_const(lines)) {
        const QString key = normalized(l.text);
        if (!l.isInstrumental && key.size() >= 2)
            ++counts[key];
    }
    int maxCount = 0;
    for (int c : std::as_const(counts))
        maxCount = std::max(maxCount, c);
    for (auto &l : lines)
        l.isChorus = maxCount > 1 && !l.isInstrumental && counts.value(normalized(l.text)) == maxCount;

    if (!doc.synced || lines.isEmpty())
        return;

    // Section threshold from the median gap between sung lines.
    QList<qint64> gaps;
    for (int i = 1; i < lines.size(); ++i)
        if (!lines[i].isInstrumental && !lines[i - 1].isInstrumental) {
            const qint64 gap = lines[i].startMs - renderEnd(lines, i - 1);
            if (gap > 0)
                gaps << gap;
        }
    std::sort(gaps.begin(), gaps.end());
    const double median = gaps.isEmpty() ? 500
                          : gaps.size() % 2 ? gaps[gaps.size() / 2]
                                            : (gaps[gaps.size() / 2 - 1] + gaps[gaps.size() / 2]) / 2.0;
    const qint64 threshold = std::clamp(qint64(median * GapMultiplier), GapMinMs, GapMaxMs);

    // Draft sections; instrumental lines are sections of their own.
    QList<QList<int>> drafts;
    QList<int> current;
    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].isInstrumental) {
            if (!current.isEmpty())
                splitOversized(lines, current, drafts);
            current.clear();
            drafts << QList<int>{i};
            continue;
        }
        if (!current.isEmpty() && lines[i].startMs - renderEnd(lines, current.last()) >= threshold) {
            splitOversized(lines, current, drafts);
            current.clear();
        }
        current << i;
    }
    if (!current.isEmpty())
        splitOversized(lines, current, drafts);

    int lastSung = -1;
    for (int d = 0; d < drafts.size(); ++d)
        if (!lines[drafts[d].first()].isInstrumental)
            lastSung = d;
    for (int d = 0; d < drafts.size(); ++d) {
        const QList<int> &sec = drafts[d];
        LyricSection s;
        s.firstLine = sec.first();
        s.lastLine = sec.last();
        s.startMs = lines[sec.first()].startMs;
        s.endMs = renderEnd(lines, sec.last());
        s.kind = lines[sec.first()].isInstrumental ? QStringLiteral("break") : classify(lines, sec, d == lastSung);
        for (int i : sec) {
            lines[i].section = int(doc.sections.size());
            lines[i].sectionKind = s.kind;
        }
        doc.sections << s;
    }
}

QList<LyricUnit> units(const LyricLine &line)
{
    QList<LyricUnit> out;
    const QString text = line.text;
    if (text.trimmed().isEmpty())
        return out;

    // Per-character times from the words: concatenated word texts match the line text
    // (apart from outer spaces trimmed off the line).
    QString joined;
    QList<int> wordAt;
    for (int w = 0; w < line.words.size(); ++w) {
        joined += line.words[w].text;
        for (qsizetype k = 0; k < line.words[w].text.size(); ++k)
            wordAt << w;
    }
    const qsizetype lead = joined.indexOf(text);
    const bool mapped = !line.words.isEmpty() && lead >= 0;
    auto timeRange = [&](qsizetype from, qsizetype to) -> std::pair<qint64, qint64> {
        if (mapped) {
            const int a = wordAt[std::clamp<qsizetype>(lead + from, 0, wordAt.size() - 1)];
            const int b = wordAt[std::clamp<qsizetype>(lead + to - 1, 0, wordAt.size() - 1)];
            return {line.words[a].startMs, line.words[b].endMs};
        }
        const double len = std::max<qsizetype>(1, text.size());
        const double dur = double(line.endMs - line.startMs);
        return {line.startMs + qint64(dur * from / len), line.startMs + qint64(dur * to / len)};
    };

    // ICU word boundaries: dictionary based for Chinese / Japanese (Qt's own boundary finder
    // treats every Han character as a word). One iterator per thread, created once.
    thread_local std::unique_ptr<icu::BreakIterator> iter = [] {
        UErrorCode status = U_ZERO_ERROR;
        std::unique_ptr<icu::BreakIterator> it(icu::BreakIterator::createWordInstance(icu::Locale("zh"), status));
        return U_SUCCESS(status) ? std::move(it) : nullptr;
    }();
    qsizetype prev = 0;
    QString pendingPrefix; // opening punctuation waiting for the next unit
    qsizetype pendingFrom = -1;
    auto addSegment = [&](qsizetype from, qsizetype to) {
        const QString seg = text.mid(from, to - from);
        if (seg.trimmed().isEmpty()) {
            if (!out.isEmpty())
                out.last().spaceAfter = true;
            return;
        }
        if (!isWordLike(seg)) {
            static const QString opening = QStringLiteral("「『（(【《〈“‘[{");
            if (out.isEmpty() || opening.contains(seg.front())) {
                if (pendingPrefix.isEmpty())
                    pendingFrom = from;
                pendingPrefix += seg;
            } else {
                out.last().text += seg;
                out.last().endMs = std::max(out.last().endMs, timeRange(from, to).second);
            }
            return;
        }
        const qsizetype unitFrom = pendingPrefix.isEmpty() ? from : pendingFrom;
        const auto [start, end] = timeRange(unitFrom, to);
        out.append({pendingPrefix + seg, start, end, false});
        pendingPrefix.clear();
    };
    if (iter) {
        const icu::UnicodeString ustr(reinterpret_cast<const UChar *>(text.utf16()), int32_t(text.size()));
        iter->setText(ustr);
        iter->first();
        for (int32_t pos = iter->next(); pos != icu::BreakIterator::DONE; pos = iter->next()) {
            if (pos > prev)
                addSegment(prev, pos);
            prev = pos;
        }
    } else {
        for (qsizetype i = 0; i < text.size(); ++i) // no ICU: one unit per character / space run
            addSegment(i, i + 1);
    }
    if (!pendingPrefix.isEmpty()) {
        if (out.isEmpty())
            out.append({pendingPrefix, line.startMs, line.endMs, false});
        else
            out.last().text += pendingPrefix;
    }
    return out;
}

} // namespace SongStructure
