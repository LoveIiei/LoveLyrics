// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "LyricsDocument.h"

#include <QObject>
#include <QPointer>

class LyricsModel;

// Answers "which line matters right now?" for the current position. Cheap enough to run
// every frame: one binary search, and signals only fire when an index changes.
class LyricsCursor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double positionMs READ positionMs WRITE setPositionMs NOTIFY positionChanged)
    // Line whose [start, end) contains the position, or -1 (before the first line, or in a gap).
    Q_PROPERTY(int activeIndex READ activeIndex NOTIFY activeIndexChanged)
    // Last line that has started (stays on a line through the gap after it), or -1.
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY focusIndexChanged)
    // First line that starts after the position, or -1.
    Q_PROPERTY(int nextIndex READ nextIndex NOTIFY nextIndexChanged)
    // 0..1 through the active line (0 when no line is active).
    Q_PROPERTY(double progress READ progress NOTIFY positionChanged)
    // ms until nextIndex starts (-1 if none).
    Q_PROPERTY(double timeToNextMs READ timeToNextMs NOTIFY positionChanged)

public:
    explicit LyricsCursor(QObject *parent = nullptr);

    void setModel(LyricsModel *model);

    double positionMs() const { return m_positionMs; }
    void setPositionMs(double ms);

    int activeIndex() const { return m_active; }
    int focusIndex() const { return m_focus; }
    int nextIndex() const { return m_next; }
    double progress() const { return m_progress; }
    double timeToNextMs() const { return m_timeToNext; }

    // Pure helpers (tested directly).
    static int lastStartedIndex(const QList<LyricLine> &lines, double positionMs);
    static int activeIndexAt(const QList<LyricLine> &lines, double positionMs);

signals:
    void positionChanged();
    void activeIndexChanged();
    void focusIndexChanged();
    void nextIndexChanged();

private:
    void update();

    QPointer<LyricsModel> m_model;
    double m_positionMs = 0;
    int m_active = -1;
    int m_focus = -1;
    int m_next = -1;
    double m_progress = 0;
    double m_timeToNext = -1;
};
