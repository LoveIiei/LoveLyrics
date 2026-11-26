// SPDX-License-Identifier: AGPL-3.0-or-later
// Song structure for the PV styles: chorus lines and sections ("paragraphs"), plus a seed
// so a style's random choices repeat identically on every play of the same song.
//
// Ported from Folia (AGPL-3.0, chthollyphile/folia-major):
//   src/utils/chorusDetector.ts                     the most repeated line text is the chorus
//   src/components/visualizer/paragraphStructure.ts sections split at gaps > 2.5 × the median
//                                                   gap (clamped to 1.25–3.5 s), oversized
//                                                   sections split at their largest gap, then
//                                                   classified chorus / break / outro / breath
//                                                   / lift / verse
#pragma once

#include "LyricsDocument.h"

#include <QStringList>

namespace SongStructure {

// Fills LyricLine::isChorus / section / sectionKind and LyricsDocument::sections / seed.
void analyze(LyricsDocument &doc);

// Display units of a line for word-by-word styles: Latin words, and dictionary words for
// CJK (ICU word breaking), with punctuation stuck to the unit before it. Times come from
// the line's words (real or estimated).
QList<LyricUnit> units(const LyricLine &line);

} // namespace SongStructure
