// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <QList>
#include <QMetaType>
#include <QString>

struct LyricWord
{
    QString text; // may carry a trailing space, so concatenating words gives the line text
    qint64 startMs = 0;
    qint64 endMs = 0;
};

struct LyricLine
{
    qint64 startMs = 0;
    qint64 endMs = 0;
    QString text;
    QString translation;
    QString romanization;
    QList<LyricWord> words;
    bool isInstrumental = false; // a timed gap (intro, solo, outro): show "♪" / breathing dots
    bool wordsEstimated = true;  // words were spread over the line, not read from the file
};

struct LyricsDocument
{
    QList<LyricLine> lines;
    bool synced = false;
    bool hasWordTiming = false;
    bool hasTranslation = false;
    QString source; // "sidecar", "embedded", "mpris", ...
    QString sourcePath;
    qint64 offsetMs = 0; // the [offset:] tag; already applied to every time in `lines`
    QString title;
    QString artist;
    QString album;

    bool isEmpty() const { return lines.isEmpty(); }
};

Q_DECLARE_METATYPE(LyricsDocument)
