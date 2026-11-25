// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>

namespace TextDecoding {

// Decode the raw bytes of a lyric file. Order: BOM (UTF-8/16/32) -> strict UTF-8 ->
// the best scoring of GB18030, Big5, Shift-JIS among those that decode without errors.
// Line endings are normalised to "\n". `encodingUsed` (optional) receives the name.
QString decode(const QByteArray &bytes, QString *encodingUsed = nullptr);

QString normalizeLineEndings(QString text);

} // namespace TextDecoding
