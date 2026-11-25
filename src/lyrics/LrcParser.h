// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "LyricsDocument.h"

#include <optional>

namespace LrcParser {

// Parse LRC text (plain, multi-timestamp, enhanced "<mm:ss.xx>" / A2 word timing,
// original + translation sharing a timestamp). Text without any timestamp gives an
// unsynced document whose lines are the plain text lines.
LyricsDocument parse(const QString &text);

// True if the text contains at least one LRC line timestamp.
bool looksSynced(const QString &text);

// "mm:ss", "mm:ss.xx", "mm:ss.xxx", "mm:ss:xx", "h:mm:ss.xx" (without brackets) -> ms.
std::optional<qint64> parseTimestamp(QStringView tag);

// Spread a line over words: one token per spaced word for Latin text, one per character
// for CJK (punctuation gets no time). Times are absolute ms.
QList<LyricWord> estimateWords(const QString &text, qint64 startMs, qint64 endMs);

// The end of a line-synced line: the next line's start, but a line followed by a long
// gap ends after a reading-time estimate so the gap can become an instrumental break.
qint64 estimateLineEnd(const QString &text, qint64 startMs, std::optional<qint64> nextStartMs);

} // namespace LrcParser
