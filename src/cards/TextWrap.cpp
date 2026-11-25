// SPDX-License-Identifier: AGPL-3.0-or-later
// Ported from spacedge/lyric-card-generator (AGPL-3.0), src/text.js.
#include "TextWrap.h"

#include <QRegularExpression>


namespace TextWrap {

namespace {

const QString &noStartChars()
{
    static const QString s = QStringLiteral("、。，．,.!?！？：；:;・ー…‥」』）)】》〉”’]}〕］｝〗〙〛〜～"
                                            "ぁぃぅぇぉっゃゅょゎゕゖァィゥェォッャュョヮヵヶ");
    return s;
}
const QString &noEndChars()
{
    static const QString s = QStringLiteral("「『（(【《〈“‘[{〔［｛〖〘〚");
    return s;
}

bool isSpaceTok(const QString &t)
{
    return !t.isEmpty() && t.trimmed().isEmpty();
}

bool inSet(const QString &set, const QString &tok)
{
    return tok.size() == 1 && set.contains(tok.front());
}

bool isWordChar(char32_t u)
{
    return (u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') || (u >= '0' && u <= '9') || (u >= 0xC0 && u <= 0x24F)
           || (u >= 0x1E00 && u <= 0x1EFF) || u == '\'' || u == 0x2019 || u == '-';
}

bool isExtend(char32_t u)
{
    return QChar::category(u) == QChar::Mark_NonSpacing || QChar::category(u) == QChar::Mark_SpacingCombining
           || QChar::category(u) == QChar::Mark_Enclosing || u == 0x200D || (u >= 0xFE00 && u <= 0xFE0F)
           || (u >= 0x1F3FB && u <= 0x1F3FF);
}

// Consecutive Latin word characters form one token, each space is a token, everything else
// is one token per code point (with combining marks / ZWJ sequences kept attached).
QStringList tokenize(const QString &text)
{
    QStringList tokens;
    enum Kind { None, Word, Space, Other } kind = None;
    bool joinNext = false;
    for (char32_t u : text.toUcs4()) {
        const QString ch = QString::fromUcs4(&u, 1);
        if (!tokens.isEmpty() && kind != Space && (joinNext || isExtend(u))) {
            tokens.last() += ch;
        } else if (QChar::isSpace(u)) {
            tokens << ch;
            kind = Space;
        } else if (isWordChar(u)) {
            if (kind == Word)
                tokens.last() += ch;
            else {
                tokens << ch;
                kind = Word;
            }
        } else {
            tokens << ch;
            kind = Other;
        }
        joinNext = u == 0x200D;
    }
    return tokens;
}

constexpr double Eps = 1e-9;
constexpr double PhraseBreakMin = 0.4;

QStringList layout(const Measure &measure, const QStringList &tokens, double maxWidth)
{
    QStringList lines;
    QStringList line;
    QString str;
    auto fits = [&](const QString &s) { return measure(s) <= maxWidth + Eps; };
    auto setLine = [&](const QStringList &toks) {
        qsizetype from = 0;
        while (from < toks.size() && isSpaceTok(toks[from]))
            ++from;
        line = toks.mid(from);
        str = line.join(QString());
    };
    auto emitLine = [&](const QStringList &toks) {
        const QString s = toks.join(QString()).trimmed();
        if (!s.isEmpty())
            lines << s;
    };
    auto push = [&](const QString &tok) {
        line << tok;
        str += tok;
    };
    auto splitLong = [&](const QString &tok) {
        for (char32_t u : tok.toUcs4()) {
            const QString ch = QString::fromUcs4(&u, 1);
            if (!line.isEmpty() && !fits(str + ch)) {
                emitLine(line);
                setLine({});
            }
            push(ch);
        }
    };
    auto breakLine = [&](const QString &tok) {
        bool trailingSpace = false;
        while (!line.isEmpty() && isSpaceTok(line.last())) {
            line.removeLast();
            trailingSpace = true;
        }
        str = line.join(QString());
        if (trailingSpace) {
            emitLine(line);
            setLine({});
            return;
        }
        // 1. Break at the last space when it is past 40% of the line.
        qsizetype sp = line.size() - 1;
        while (sp > 0 && !isSpaceTok(line[sp]))
            --sp;
        if (sp > 0 && measure(line.mid(0, sp).join(QString())) > PhraseBreakMin * measure(str)) {
            emitLine(line.mid(0, sp));
            setLine(line.mid(sp + 1));
            return;
        }
        // 2./3. Kinsoku: carry a line-start-forbidden token's predecessor, or a line-end-
        // forbidden token, to the next line; keep at least one token.
        if (line.size() == 1) {
            QStringList chars;
            for (char32_t u : line.first().toUcs4())
                chars << QString::fromUcs4(&u, 1);
            line = chars;
        }
        qsizetype i = line.size();
        for (bool moved = true; moved;) {
            moved = false;
            const QString nextFirst = i < line.size() ? line[i] : tok;
            if (i > 1 && inSet(noStartChars(), nextFirst)) {
                --i;
                moved = true;
            }
            if (i > 1 && inSet(noEndChars(), line[i - 1])) {
                --i;
                moved = true;
            }
        }
        emitLine(line.mid(0, i));
        setLine(line.mid(i));
    };
    auto place = [&](const QString &tok) {
        for (int guard = 0; guard < 10000; ++guard) {
            if (isSpaceTok(tok)) {
                if (line.isEmpty())
                    return;
                if (fits(str + tok))
                    push(tok);
                else {
                    emitLine(line);
                    setLine({});
                }
                return;
            }
            if (fits(str + tok)) {
                push(tok);
                return;
            }
            if (line.isEmpty()) {
                splitLong(tok);
                return;
            }
            breakLine(tok);
        }
    };
    for (const QString &t : tokens)
        place(t);
    emitLine(line);
    return lines;
}

QStringList wrapParagraph(const Measure &measure, const QString &text, double maxWidth)
{
    const QStringList tokens = tokenize(text);
    const QStringList lines = layout(measure, tokens, maxWidth);
    const qsizetype count = lines.size();
    if (count < 2 || lines.last().toUcs4().size() > 2)
        return lines;
    // Orphan: find the narrowest width that keeps the same number of lines.
    double lo = maxWidth * 0.5, hi = maxWidth;
    QStringList best = lines;
    for (int i = 0; i < 14; ++i) {
        const double mid = (lo + hi) / 2;
        const QStringList trial = layout(measure, tokens, mid);
        if (trial.size() == count) {
            hi = mid;
            best = trial;
        } else {
            lo = mid;
        }
    }
    return best;
}

} // namespace

bool hasKana(const QString &s)
{
    for (char32_t u : s.toUcs4())
        if ((u >= 0x3041 && u <= 0x3096) || (u >= 0x309D && u <= 0x309F) || (u >= 0x30A1 && u <= 0x30FA)
            || (u >= 0x30FC && u <= 0x30FF) || (u >= 0x31F0 && u <= 0x31FF) || (u >= 0xFF66 && u <= 0xFF9F))
            return true;
    return false;
}

bool hasHan(const QString &s)
{
    for (char32_t u : s.toUcs4())
        if (QChar::script(u) == QChar::Script_Han)
            return true;
    return false;
}

bool hasHangul(const QString &s)
{
    for (char32_t u : s.toUcs4())
        if (QChar::script(u) == QChar::Script_Hangul)
            return true;
    return false;
}

QStringList wrap(const Measure &measure, const QString &text, double maxWidth)
{
    QStringList out;
    for (const QString &para : text.split(QLatin1Char('\n')))
        out << wrapParagraph(measure, para, maxWidth);
    return out;
}

} // namespace TextWrap
