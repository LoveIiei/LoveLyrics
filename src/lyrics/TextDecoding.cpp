// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Decoding order follows Folia's src/utils/lyrics/lyricFileDecoding.ts (BOM, then strict
// UTF-8, then GB18030, and "\r\n" / "\r" -> "\n"), extended with Big5 and Shift-JIS and a
// small score to choose between legacy CJK encodings.
#include "TextDecoding.h"

#include <QStringDecoder>

namespace {

bool decodeStrict(QStringDecoder &decoder, const QByteArray &bytes, QString &out)
{
    if (!decoder.isValid())
        return false;
    out = decoder.decode(bytes);
    return !decoder.hasError();
}

// Rough "does this look like real text" score for a legacy CJK decoding. Wrong guesses tend to
// produce half-width katakana (GBK read as Shift-JIS), private-use or control characters,
// while right guesses produce kana (Japanese) or common CJK ideographs.
int plausibility(const QString &text)
{
    int score = 0;
    for (const QChar c : text) {
        const char16_t u = c.unicode();
        if (u < 0x80)
            continue;
        if (u >= 0x3040 && u <= 0x30ff)
            score += 3; // hiragana / katakana
        else if (u >= 0x4e00 && u <= 0x9fff)
            score += 1; // CJK unified ideographs
        else if (u >= 0x3000 && u <= 0x303f)
            score += 1; // CJK punctuation
        else if (u >= 0xff01 && u <= 0xff5e)
            score += 1; // full-width ASCII
        else if (u >= 0xff61 && u <= 0xff9f)
            score -= 4; // half-width katakana
        else if ((u >= 0xe000 && u <= 0xf8ff) || u < 0xa0)
            score -= 6; // private use / C1 controls
        else if (c.isSurrogate())
            score -= 1; // rare extension planes
    }
    return score;
}

} // namespace

namespace TextDecoding {

QString normalizeLineEndings(QString text)
{
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text;
}

QString decode(const QByteArray &bytes, QString *encodingUsed)
{
    auto setUsed = [&](const char *name) {
        if (encodingUsed)
            *encodingUsed = QString::fromLatin1(name);
    };

    // 1. A BOM wins.
    if (const auto bom = QStringConverter::encodingForData(bytes)) {
        QStringDecoder decoder(*bom);
        setUsed(QStringConverter::nameForEncoding(*bom));
        QString text = decoder.decode(bytes);
        if (text.startsWith(QChar(0xfeff)))
            text.remove(0, 1);
        return normalizeLineEndings(text);
    }

    // 2. Strict UTF-8 (plain ASCII lands here too).
    {
        QStringDecoder utf8(QStringDecoder::Utf8);
        QString text;
        if (decodeStrict(utf8, bytes, text)) {
            setUsed("UTF-8");
            return normalizeLineEndings(text);
        }
    }

    // 3. Legacy CJK encodings: keep the best-looking clean decoding. Ties go to the earlier
    //    entry, so GB18030 (the most common case for .lrc files) wins when unsure.
    static const char *const candidates[] = {"GB18030", "Big5", "Shift_JIS"};
    QString best;
    const char *bestName = nullptr;
    int bestScore = 0;
    for (const char *name : candidates) {
        QStringDecoder decoder(name);
        QString text;
        if (!decodeStrict(decoder, bytes, text))
            continue;
        const int score = plausibility(text);
        if (!bestName || score > bestScore) {
            best = text;
            bestName = name;
            bestScore = score;
        }
    }
    if (bestName) {
        setUsed(bestName);
        return normalizeLineEndings(best);
    }

    // 4. Nothing decodes cleanly: lenient UTF-8 (invalid bytes become U+FFFD).
    setUsed("UTF-8 (lenient)");
    return normalizeLineEndings(QString::fromUtf8(bytes));
}

} // namespace TextDecoding
