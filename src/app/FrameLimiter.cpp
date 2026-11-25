// SPDX-License-Identifier: AGPL-3.0-or-later
#include "FrameLimiter.h"

#include <QQuickWindow>
#include <QThread>

namespace {
// Wake a little early: the present after the sleep still waits for vsync.
constexpr qint64 SlackNs = 1'500'000;
} // namespace

FrameLimiter::FrameLimiter(QQuickWindow *window, QObject *parent)
    : QObject(parent)
{
    // Emitted on the render thread (threaded render loop), after the frame was presented.
    connect(window, &QQuickWindow::afterFrameEnd, this, [this] { afterFrame(); }, Qt::DirectConnection);
}

void FrameLimiter::setCap(int fps)
{
    m_cap = std::max(0, fps);
    m_intervalNs.store(m_cap > 0 ? 1'000'000'000 / m_cap : 0);
}

void FrameLimiter::afterFrame()
{
    const qint64 interval = m_intervalNs.load();
    if (interval <= 0) {
        m_deadlineNs = 0;
        return;
    }
    if (!m_timer.isValid())
        m_timer.start();

    const qint64 now = m_timer.nsecsElapsed();
    if (m_deadlineNs == 0 || now > m_deadlineNs + interval) {
        // First frame, or we were idle (no frames for a while): start a new schedule.
        m_deadlineNs = now + interval;
        return;
    }
    const qint64 wait = m_deadlineNs - SlackNs - now;
    if (wait > 0)
        QThread::usleep(static_cast<unsigned long>(wait / 1000));
    m_deadlineNs += interval;
}
