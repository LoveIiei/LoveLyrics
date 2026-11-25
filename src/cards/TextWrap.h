// SPDX-License-Identifier: AGPL-3.0-or-later
// Line breaking for mixed CJK / Latin text, ported from spacedge/lyric-card-generator
// (AGPL-3.0), src/text.js: Latin words stay whole, breaks prefer spaces past 40% of the
// line, kinsoku rules (no line-start punctuation / small kana, no line-end opening
// brackets), and no orphan of 1–2 characters on the last line.
#pragma once

#include <QString>
#include <QStringList>

#include <functional>

namespace TextWrap {

using Measure = std::function<double(const QString &)>;

bool hasKana(const QString &s);
bool hasHan(const QString &s);
bool hasHangul(const QString &s);

// Wraps text (may contain "\n" paragraph breaks) into lines no wider than maxWidth.
QStringList wrap(const Measure &measure, const QString &text, double maxWidth);

} // namespace TextWrap
