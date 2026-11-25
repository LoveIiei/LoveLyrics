// SPDX-License-Identifier: AGPL-3.0-or-later
#include "TrackResolver.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>

namespace TrackResolver {

QString localPath(const QString &url)
{
    if (url.isEmpty())
        return {};
    if (url.startsWith(QLatin1Char('/')))
        return url; // some players send a bare path
    const QUrl u(url, QUrl::TolerantMode);
    if (!u.isLocalFile())
        return {};
    return u.toLocalFile();
}

QString findSidecarLyrics(const QString &audioPath)
{
    if (audioPath.isEmpty())
        return {};
    const QFileInfo audio(audioPath);
    const QDir dir = audio.absoluteDir();
    if (!dir.exists())
        return {};

    const QString stem = audio.completeBaseName();
    const QString fileName = audio.fileName();
    QString byFileName;

    // Name filters are case-insensitive by default, so this also finds "*.LRC".
    const QStringList candidates = dir.entryList({QStringLiteral("*.lrc")}, QDir::Files | QDir::Readable);
    for (const QString &name : candidates) {
        const QString candidateStem = QFileInfo(name).completeBaseName();
        if (candidateStem.compare(stem, Qt::CaseInsensitive) == 0)
            return dir.filePath(name);
        if (byFileName.isEmpty() && candidateStem.compare(fileName, Qt::CaseInsensitive) == 0)
            byFileName = dir.filePath(name);
    }
    return byFileName;
}

QString findFolderCover(const QString &audioPath)
{
    if (audioPath.isEmpty())
        return {};
    const QDir dir = QFileInfo(audioPath).absoluteDir();
    if (!dir.exists())
        return {};

    static const QStringList preferredStems = {
        QStringLiteral("cover"), QStringLiteral("folder"), QStringLiteral("front"),
        QStringLiteral("album"), QStringLiteral("albumart"),
    };
    const QStringList images = dir.entryList(
        {QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.png"), QStringLiteral("*.webp")},
        QDir::Files | QDir::Readable, QDir::Name);

    for (const QString &stem : preferredStems) {
        for (const QString &name : images) {
            if (QFileInfo(name).completeBaseName().compare(stem, Qt::CaseInsensitive) == 0)
                return dir.filePath(name);
        }
    }
    for (const QString &name : images) {
        if (name.startsWith(QLatin1String("albumart"), Qt::CaseInsensitive))
            return dir.filePath(name);
    }
    return {};
}

} // namespace TrackResolver
