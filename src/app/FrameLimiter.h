// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <QElapsedTimer>
#include <QObject>

#include <atomic>

class QQuickWindow;

// Caps a Qt Quick window's frame rate (to save battery on high refresh panels).
// Qt Quick has no built-in cap, so after each frame the render thread sleeps until the
// next frame is due; the following present then lands on the next vsync after that.
// 0 = no cap.
class FrameLimiter : public QObject
{
    Q_OBJECT

public:
    explicit FrameLimiter(QQuickWindow *window, QObject *parent = nullptr);
    void setCap(int fps);
    int cap() const { return m_cap; }

private:
    void afterFrame(); // render thread

    int m_cap = 0;
    std::atomic<qint64> m_intervalNs{0};
    // Render thread only:
    QElapsedTimer m_timer;
    qint64 m_deadlineNs = 0;
};
