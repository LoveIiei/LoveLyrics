// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Active / upcoming line rules follow Folia's src/components/visualizer/runtime.ts
// (AGPL-3.0, chthollyphile/folia-major), re-implemented with a binary search.
#include "LyricsCursor.h"

#include "LyricsModel.h"

#include <algorithm>

LyricsCursor::LyricsCursor(QObject *parent)
    : QObject(parent)
{
}

void LyricsCursor::setModel(LyricsModel *model)
{
    if (m_model)
        disconnect(m_model, nullptr, this, nullptr);
    m_model = model;
    if (m_model)
        connect(m_model, &LyricsModel::documentChanged, this, &LyricsCursor::update);
    update();
}

void LyricsCursor::setPositionMs(double ms)
{
    m_positionMs = ms;
    update();
}

int LyricsCursor::lastStartedIndex(const QList<LyricLine> &lines, double positionMs)
{
    // First line with startMs > position, minus one.
    const auto it = std::upper_bound(lines.cbegin(), lines.cend(), positionMs,
                                     [](double pos, const LyricLine &l) { return pos < double(l.startMs); });
    return int(it - lines.cbegin()) - 1;
}

int LyricsCursor::activeIndexAt(const QList<LyricLine> &lines, double positionMs)
{
    const int i = lastStartedIndex(lines, positionMs);
    if (i < 0)
        return -1;
    return positionMs < double(lines[i].endMs) ? i : -1;
}

void LyricsCursor::update()
{
    int active = -1;
    int focus = -1;
    int next = -1;
    double progress = 0;
    double timeToNext = -1;

    if (m_model && m_model->synced()) {
        const auto &lines = m_model->document().lines;
        focus = lastStartedIndex(lines, m_positionMs);
        if (focus >= 0 && m_positionMs < double(lines[focus].endMs)) {
            active = focus;
            const double len = std::max<double>(1.0, lines[focus].endMs - lines[focus].startMs);
            progress = std::clamp((m_positionMs - lines[focus].startMs) / len, 0.0, 1.0);
        }
        if (focus + 1 < lines.size()) {
            next = focus + 1;
            timeToNext = lines[next].startMs - m_positionMs;
        }
    }

    const bool activeChanged = active != m_active;
    const bool focusChanged = focus != m_focus;
    const bool nextChanged = next != m_next;
    m_active = active;
    m_focus = focus;
    m_next = next;
    m_progress = progress;
    m_timeToNext = timeToNext;

    if (activeChanged)
        emit activeIndexChanged();
    if (focusChanged)
        emit focusIndexChanged();
    if (nextChanged)
        emit nextIndexChanged();
    emit positionChanged();
}
