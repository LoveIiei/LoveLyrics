// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>

// Thin TagLib wrapper. Safe to call from worker threads (each call opens its own file).
namespace TagReader {

// Lyrics stored in the file's tags, as LRC text when synced:
//   ID3v2 SYLT (converted to LRC) -> LYRICS / UNSYNCEDLYRICS / LYRICS:<desc> properties
//   (FLAC/Ogg Vorbis comments, ID3v2 USLT, MP4 ©lyr, APE ...). Synced text wins.
QString readLyrics(const QString &path);

// The front cover (or first picture) embedded in the file, encoded image bytes.
QByteArray readCover(const QString &path);

} // namespace TagReader
