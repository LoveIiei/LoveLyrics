// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "LyricsDocument.h"
#include "mpris/MprisClient.h"

#include <QObject>
#include <QStringList>

// Finds and parses lyrics for a track on a worker thread. Each request carries an id, so a
// slow load for an old track can never replace the current track's lyrics.
class LyricsLoader : public QObject
{
    Q_OBJECT

public:
    static const QStringList DefaultSourceOrder; // {"sidecar", "embedded", "mpris"}

    explicit LyricsLoader(QObject *parent = nullptr);

    void setSourceOrder(const QStringList &order);
    QStringList sourceOrder() const { return m_order; }

    // Async. Emits loaded() unless another load() / cancel() came first.
    void load(const TrackInfo &track);
    void cancel();

    // The synchronous lookup used by load(); public for tests.
    static LyricsDocument loadNow(const TrackInfo &track, const QStringList &order);

signals:
    void loaded(const LyricsDocument &doc);
    void loadingChanged(bool loading);

private:
    QStringList m_order = DefaultSourceOrder + QStringList{QStringLiteral("cache")};
    quint64 m_request = 0;
    int m_pending = 0;
};
