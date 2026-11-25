// SPDX-License-Identifier: AGPL-3.0-or-later
//
// LRC parsing. Logic follows ideas from Folia (AGPL-3.0, chthollyphile/folia-major):
//   src/utils/lyrics/parserCore.ts          timestamp forms, enhanced/A2 word markers
//   src/utils/lyrics/timelineSplitter.ts    original + translation on the same timestamp,
//                                           blank timed lines never take the original's slot,
//                                           romanization detection
//   src/utils/lyrics/estimatedLineTiming.ts line end estimation, weighted word timing
// Re-implemented in C++; timings are in milliseconds here.
#include "LrcParser.h"

#include "TextDecoding.h"

#include <QRegularExpression>
#include <QTextBoundaryFinder>

#include <algorithm>

namespace {

constexpr qint64 IntroGapMs = 4000;        // first line later than this -> leading "♪" line
constexpr qint64 InstrumentalGapMs = 6000; // silence between lines longer than this -> "♪" line

const QRegularExpression &leadingTagRe()
{
    static const QRegularExpression re(QStringLiteral(R"(\[([^\[\]]*)\])"));
    return re;
}

const QRegularExpression &metadataRe()
{
    static const QRegularExpression re(QStringLiteral(R"(^([A-Za-z#]+)\s*:(.*)$)"));
    return re;
}

// Inline word markers: <mm:ss.xx> (enhanced LRC) or [mm:ss.xx] inside the body.
const QRegularExpression &inlineMarkerRe()
{
    static const QRegularExpression re(
        QStringLiteral(R"([<\[](\d+:\d{1,2}(?::\d{1,2})?(?:[.:]\d{1,3})?)[>\]])"));
    return re;
}

const QRegularExpression &anyTimestampRe()
{
    static const QRegularExpression re(QStringLiteral(R"(\[\d+:\d{1,2}(?:[.:]\d{1,3})?\])"));
    return re;
}

bool isCjk(char32_t u)
{
    return (u >= 0x3040 && u <= 0x30ff)     // kana
           || (u >= 0x3400 && u <= 0x4dbf)  // CJK ext A
           || (u >= 0x4e00 && u <= 0x9fff)  // CJK unified
           || (u >= 0xf900 && u <= 0xfaff)  // compatibility ideographs
           || (u >= 0xac00 && u <= 0xd7af)  // hangul syllables
           || (u >= 0x20000 && u <= 0x3134f); // ext B..G
}

bool hasCjk(QStringView s)
{
    for (char32_t u : s.toUcs4())
        if (isCjk(u))
            return true;
    return false;
}

bool isCjkPunctuation(QStringView g)
{
    static const QString punct = QStringLiteral("，。！？、：；“”‘’（）《》「」『』…—～·,.!?;:\"'()");
    return g.size() == 1 && punct.contains(g.front());
}

// Text that is mostly Latin letters and has no CJK: a romanization line under a CJK original.
bool isRomanizationCandidate(const QString &text)
{
    if (hasCjk(text))
        return false;
    int letters = 0;
    int nonSpace = 0;
    for (const QChar c : text) {
        if (c.isSpace())
            continue;
        ++nonSpace;
        if (c.isLetter() && c.unicode() < 0x250)
            ++letters;
    }
    return letters >= 2 && letters * 100 >= nonSpace * 45;
}

QStringList graphemes(const QString &text)
{
    QStringList out;
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
    qsizetype prev = 0;
    while (finder.toNextBoundary() != -1) {
        const qsizetype pos = finder.position();
        if (pos > prev)
            out << text.mid(prev, pos - prev);
        prev = pos;
    }
    return out;
}

qint64 readingTimeMs(const QString &text)
{
    qint64 ms = 0;
    for (char32_t u : QStringView(text).toUcs4()) {
        if (isCjk(u))
            ms += 400;
        else if (!QChar::isSpace(u))
            ms += 120;
    }
    return ms;
}

struct Entry
{
    qint64 timeMs = 0;
    QString text;
    QList<LyricWord> words; // endMs == -1 means "until the next word / line end"
    bool wordTimed = false;
    bool blank = false;
};

// Split a body with inline time markers into words. Text before the first marker starts at
// the line time (A2 style: "Hello<00:12.50> world").
void parseWords(const QString &body, qint64 lineStartMs, Entry &entry)
{
    QList<LyricWord> words;
    qint64 currentStart = lineStartMs;
    qsizetype pos = 0;
    bool anyMarker = false;

    auto it = inlineMarkerRe().globalMatch(body);
    while (it.hasNext()) {
        const auto m = it.next();
        const auto time = LrcParser::parseTimestamp(m.capturedView(1));
        if (!time)
            continue;
        anyMarker = true;
        const QString segment = body.mid(pos, m.capturedStart() - pos);
        if (!segment.isEmpty())
            words.append({segment, currentStart, -1});
        if (!words.isEmpty() && words.last().endMs < 0)
            words.last().endMs = *time;
        currentStart = *time;
        pos = m.capturedEnd();
    }

    if (!anyMarker) {
        entry.text = body.trimmed();
        entry.blank = entry.text.isEmpty();
        return;
    }

    const QString tail = body.mid(pos);
    if (!tail.isEmpty())
        words.append({tail, currentStart, -1});

    // Move leading spaces onto the previous word so each word reads "word ".
    for (qsizetype i = 1; i < words.size(); ++i) {
        qsizetype lead = 0;
        while (lead < words[i].text.size() && words[i].text[lead].isSpace())
            ++lead;
        if (lead > 0) {
            words[i - 1].text += words[i].text.left(lead);
            words[i].text.remove(0, lead);
        }
    }
    words.removeIf([](const LyricWord &w) { return w.text.isEmpty(); });
    if (!words.isEmpty()) {
        // Trim the outer edges of the line.
        while (!words.first().text.isEmpty() && words.first().text.front().isSpace())
            words.first().text.remove(0, 1);
        while (!words.last().text.isEmpty() && words.last().text.back().isSpace())
            words.last().text.chop(1);
        words.removeIf([](const LyricWord &w) { return w.text.isEmpty(); });
    }

    QString text;
    for (const auto &w : std::as_const(words))
        text += w.text;
    entry.text = text.trimmed();
    entry.blank = entry.text.isEmpty();
    entry.wordTimed = !entry.blank;
    entry.words = entry.blank ? QList<LyricWord>{} : words;
}

LyricsDocument buildUnsynced(const QStringList &plainLines)
{
    LyricsDocument doc;
    doc.synced = false;
    bool lastBlank = true; // drops leading blanks
    for (const QString &line : plainLines) {
        const bool blank = line.isEmpty();
        if (blank && lastBlank)
            continue;
        LyricLine l;
        l.text = line;
        doc.lines.append(l);
        lastBlank = blank;
    }
    while (!doc.lines.isEmpty() && doc.lines.last().text.isEmpty())
        doc.lines.removeLast();
    return doc;
}

LyricLine instrumentalLine(qint64 startMs, qint64 endMs)
{
    LyricLine l;
    l.startMs = startMs;
    l.endMs = endMs;
    l.isInstrumental = true;
    l.wordsEstimated = false;
    return l;
}

} // namespace

namespace LrcParser {

std::optional<qint64> parseTimestamp(QStringView tag)
{
    static const QRegularExpression hourRe(QStringLiteral(R"(^(\d+):(\d{1,2}):(\d{1,2})\.(\d{1,3})$)"));
    static const QRegularExpression re(QStringLiteral(R"(^(\d+):(\d{1,2})(?:[.:](\d{1,3}))?$)"));

    auto fraction = [](QStringView digits) -> qint64 {
        if (digits.isEmpty())
            return 0;
        const qint64 v = digits.toLongLong();
        switch (digits.size()) {
        case 1: return v * 100;
        case 2: return v * 10;
        default: return v;
        }
    };

    const QString s = tag.trimmed().toString();
    if (const auto m = hourRe.match(s); m.hasMatch()) {
        return m.capturedView(1).toLongLong() * 3600000 + m.capturedView(2).toLongLong() * 60000
               + m.capturedView(3).toLongLong() * 1000 + fraction(m.capturedView(4));
    }
    if (const auto m = re.match(s); m.hasMatch()) {
        return m.capturedView(1).toLongLong() * 60000 + m.capturedView(2).toLongLong() * 1000
               + fraction(m.capturedView(3));
    }
    return std::nullopt;
}

bool looksSynced(const QString &text)
{
    return anyTimestampRe().match(text).hasMatch();
}

qint64 estimateLineEnd(const QString &text, qint64 startMs, std::optional<qint64> nextStartMs)
{
    const qint64 reading = readingTimeMs(text) + 2000;
    if (!nextStartMs)
        return startMs + std::clamp<qint64>(reading, 3000, 8000);
    const qint64 duration = *nextStartMs - startMs;
    if (duration > reading && duration > 5000)
        return startMs + std::max<qint64>(reading, 5000);
    return startMs + std::max<qint64>(duration, 0);
}

QList<LyricWord> estimateWords(const QString &text, qint64 startMs, qint64 endMs)
{
    struct Token
    {
        QString text;
        double weight;
    };
    QList<Token> tokens;
    double totalWeight = 0;

    static const QRegularExpression wordRe(QStringLiteral(R"(\S+\s*)"));
    auto it = wordRe.globalMatch(text);
    while (it.hasNext()) {
        const QString token = it.next().captured(0);
        if (hasCjk(token)) {
            const QStringList gs = graphemes(token);
            for (const QString &g : gs) {
                if (g.trimmed().isEmpty() && !tokens.isEmpty()) {
                    tokens.last().text += g; // keep spaces attached
                    continue;
                }
                const double w = isCjkPunctuation(g.trimmed()) ? 0.0 : 1.0;
                tokens.append({g, w});
                totalWeight += w;
            }
        } else {
            const double w = 1.0 + token.trimmed().size() * 0.15;
            tokens.append({token, w});
            totalWeight += w;
        }
    }
    if (tokens.isEmpty())
        return {};
    if (totalWeight <= 0)
        totalWeight = 1;

    const double duration = std::max<double>(endMs - startMs, 100);
    const double perWeight = duration * 0.9 / totalWeight;
    QList<LyricWord> words;
    double t = startMs;
    for (const auto &tok : std::as_const(tokens)) {
        const double len = tok.weight * perWeight;
        const double shown = std::max(len, 50.0);
        words.append({tok.text, qint64(t), qint64(t + shown)});
        t += tok.weight > 0 ? len : 50.0;
    }
    if (words.last().endMs > endMs && words.last().endMs > startMs) {
        const double scale = double(endMs - startMs) / double(words.last().endMs - startMs);
        for (auto &w : words) {
            w.startMs = startMs + qint64((w.startMs - startMs) * scale);
            w.endMs = startMs + qint64((w.endMs - startMs) * scale);
        }
    }
    words.last().text = words.last().text.trimmed();
    return words;
}

LyricsDocument parse(const QString &input)
{
    QString text = TextDecoding::normalizeLineEndings(input);
    if (text.startsWith(QChar(0xfeff)))
        text.remove(0, 1);

    LyricsDocument doc;
    QList<Entry> entries;
    QStringList plainLines;
    qint64 offset = 0;

    const QStringList rawLines = text.split(QLatin1Char('\n'));
    for (const QString &raw : rawLines) {
        const QString line = raw.trimmed();
        if (line.isEmpty()) {
            plainLines.append(QString());
            continue;
        }

        QList<qint64> times;
        qsizetype pos = 0;
        bool sawMetadata = false;
        while (pos < line.size()) {
            const auto m = leadingTagRe().match(line, pos, QRegularExpression::NormalMatch,
                                                QRegularExpression::AnchorAtOffsetMatchOption);
            if (!m.hasMatch())
                break;
            const QString content = m.captured(1);
            if (const auto t = parseTimestamp(content)) {
                times.append(*t);
                pos = m.capturedEnd();
                // Tolerate spaces between stacked timestamps: "[00:01.00] [00:30.00]text"
                while (pos < line.size() && line[pos] == QLatin1Char(' ')
                       && pos + 1 < line.size() && line[pos + 1] == QLatin1Char('['))
                    ++pos;
                continue;
            }
            if (times.isEmpty()) {
                if (const auto mm = metadataRe().match(content); mm.hasMatch()) {
                    const QString key = mm.captured(1).toLower();
                    const QString value = mm.captured(2).trimmed();
                    if (key == u"ti")
                        doc.title = value;
                    else if (key == u"ar")
                        doc.artist = value;
                    else if (key == u"al")
                        doc.album = value;
                    else if (key == u"offset")
                        offset = value.toLongLong();
                    sawMetadata = true;
                    pos = m.capturedEnd();
                    continue;
                }
            }
            break;
        }

        const QString body = line.mid(pos);
        if (times.isEmpty()) {
            if (!(sawMetadata && body.trimmed().isEmpty()))
                plainLines.append(line);
            continue;
        }

        Entry base;
        base.timeMs = times.first();
        parseWords(body, times.first(), base);
        for (const qint64 t : std::as_const(times)) {
            Entry e = base;
            e.timeMs = t;
            const qint64 delta = t - times.first();
            for (auto &w : e.words) {
                w.startMs += delta;
                if (w.endMs >= 0)
                    w.endMs += delta;
            }
            entries.append(e);
        }
    }

    if (entries.isEmpty()) {
        LyricsDocument plain = buildUnsynced(plainLines);
        plain.title = doc.title;
        plain.artist = doc.artist;
        plain.album = doc.album;
        return plain;
    }

    doc.synced = true;
    doc.offsetMs = offset;
    // [offset:+n] makes lyrics appear n ms sooner.
    if (offset != 0) {
        for (auto &e : entries) {
            e.timeMs = std::max<qint64>(0, e.timeMs - offset);
            for (auto &w : e.words) {
                w.startMs = std::max<qint64>(0, w.startMs - offset);
                if (w.endMs >= 0)
                    w.endMs = std::max<qint64>(0, w.endMs - offset);
            }
        }
    }

    std::stable_sort(entries.begin(), entries.end(),
                     [](const Entry &a, const Entry &b) { return a.timeMs < b.timeMs; });

    // Group entries sharing a timestamp: original, then translation (and maybe romanization).
    struct Item
    {
        Entry main;
        QString translation;
        QString romanization;
    };
    QList<Item> items;
    for (qsizetype i = 0; i < entries.size();) {
        qsizetype j = i + 1;
        while (j < entries.size() && entries[j].timeMs == entries[i].timeMs)
            ++j;
        QList<Entry> members;
        for (qsizetype k = i; k < j; ++k)
            if (!entries[k].blank)
                members.append(entries[k]);
        if (members.isEmpty()) {
            items.append({entries[i], {}, {}});
        } else {
            Item item{members.first(), {}, {}};
            const QList<Entry> alternates = members.mid(1);
            int romanIndex = -1;
            if (hasCjk(item.main.text)) {
                int candidates = 0;
                for (int k = 0; k < alternates.size(); ++k)
                    if (isRomanizationCandidate(alternates[k].text)) {
                        ++candidates;
                        romanIndex = k;
                    }
                if (candidates != 1 || alternates.size() < 2)
                    romanIndex = -1; // a lone Latin alternate is more likely a translation
            }
            for (int k = 0; k < alternates.size(); ++k) {
                if (k == romanIndex)
                    item.romanization = alternates[k].text;
                else if (item.translation.isEmpty())
                    item.translation = alternates[k].text;
            }
            items.append(item);
        }
        i = j;
    }

    // Lines with end times; blank items only mark where the previous line stops.
    QList<LyricLine> lines;
    for (qsizetype i = 0; i < items.size(); ++i) {
        const Item &item = items[i];
        if (item.main.blank)
            continue;
        const std::optional<qint64> next =
            i + 1 < items.size() ? std::optional<qint64>(items[i + 1].main.timeMs) : std::nullopt;

        LyricLine line;
        line.startMs = item.main.timeMs;
        line.text = item.main.text;
        line.translation = item.translation;
        line.romanization = item.romanization;

        if (item.main.wordTimed) {
            line.words = item.main.words;
            line.wordsEstimated = false;
            doc.hasWordTiming = true;
            qint64 end = line.words.last().endMs;
            if (end < 0) {
                // Last word has no closing marker: let it run to the next line (or a short guess).
                end = next ? *next : line.words.last().startMs + 1000;
                end = std::max(end, line.words.last().startMs + 200);
                line.words.last().endMs = end;
            }
            line.endMs = std::max(end, line.startMs + 1);
        } else {
            line.endMs = std::max(estimateLineEnd(line.text, line.startMs, next), line.startMs + 1);
            line.words = estimateWords(line.text, line.startMs, line.endMs);
            line.wordsEstimated = true;
        }
        if (!line.translation.isEmpty())
            doc.hasTranslation = true;
        lines.append(line);
    }

    // Instrumental breaks: intro and long gaps between lines.
    QList<LyricLine> withBreaks;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (i == 0 && lines[i].startMs >= IntroGapMs)
            withBreaks.append(instrumentalLine(0, lines[i].startMs));
        withBreaks.append(lines[i]);
        if (i + 1 < lines.size() && lines[i + 1].startMs - lines[i].endMs >= InstrumentalGapMs)
            withBreaks.append(instrumentalLine(lines[i].endMs, lines[i + 1].startMs));
    }
    doc.lines = withBreaks;
    return doc;
}

} // namespace LrcParser
