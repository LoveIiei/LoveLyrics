// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace TrackResolver {

// xesam:url -> local file path. Handles percent-encoding; "" for non-file URLs.
QString localPath(const QString &url);

// Sidecar lyric file for an audio file: "Song.lrc" (stem matched without caring about case),
// then "Song.flac.lrc". "" if none. The audio file itself does not need to exist.
QString findSidecarLyrics(const QString &audioPath);

// True for an existing regular file (not a directory, FIFO, socket or device).
bool isRegularFile(const QString &path);

// cover.* / folder.* / front.* / albumart*.* next to the audio file, "" if none.
QString findFolderCover(const QString &audioPath);

} // namespace TrackResolver
