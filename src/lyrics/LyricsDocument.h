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

// A display unit (a word, or a CJK dictionary word) for word-by-word styles.
struct LyricUnit
{
    QString text;
    qint64 startMs = 0;
    qint64 endMs = 0;
    bool spaceAfter = false;
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
    // Song structure (SongStructure::analyze):
    bool isChorus = false;
    int section = -1;
    QString sectionKind; // chorus, verse, lift, breath, break, outro
};

struct LyricSection
{
    int firstLine = 0;
    int lastLine = 0;
    qint64 startMs = 0;
    qint64 endMs = 0;
    QString kind;
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
    QList<LyricSection> sections;
    quint32 seed = 0; // stable per song: styles seed their random choices with it

    bool isEmpty() const { return lines.isEmpty(); }
};

Q_DECLARE_METATYPE(LyricsDocument)
