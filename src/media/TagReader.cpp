// SPDX-License-Identifier: AGPL-3.0-or-later
#include "TagReader.h"

#include "lyrics/LrcParser.h"

#include <QFile>
#include <QLoggingCategory>

#include <aifffile.h>
#include <fileref.h>
#include <id3v2tag.h>
#include <mpegfile.h>
#include <synchronizedlyricsframe.h>
#include <tpropertymap.h>
#include <tvariant.h>
#include <wavfile.h>

Q_LOGGING_CATEGORY(lcTags, "lovelyrics.tags")

namespace {

QString toQString(const TagLib::String &s)
{
    return QString::fromStdString(s.to8Bit(true));
}

QString lrcTime(unsigned int ms)
{
    return QStringLiteral("%1:%2.%3")
        .arg(ms / 60000, 2, 10, QLatin1Char('0'))
        .arg((ms / 1000) % 60, 2, 10, QLatin1Char('0'))
        .arg(ms % 1000, 3, 10, QLatin1Char('0'));
}

// SYLT -> LRC. One entry per line is the usual layout; when entries start with a newline,
// they are syllables/words of a line and become enhanced LRC word markers.
QString syltToLrc(const TagLib::ID3v2::SynchronizedLyricsFrame *frame)
{
    const auto list = frame->synchedText();
    if (list.isEmpty())
        return {};
    bool wordLevel = false;
    for (const auto &entry : list) {
        const QString t = toQString(entry.text);
        if (t.startsWith(QLatin1Char('\n')) || t.startsWith(QLatin1Char('\r'))) {
            wordLevel = true;
            break;
        }
    }

    QString out;
    if (!wordLevel) {
        for (const auto &entry : list)
            out += QStringLiteral("[%1]%2\n").arg(lrcTime(entry.time), toQString(entry.text).trimmed());
        return out;
    }

    bool first = true;
    for (const auto &entry : list) {
        QString t = toQString(entry.text);
        const bool newLine = first || t.startsWith(QLatin1Char('\n')) || t.startsWith(QLatin1Char('\r'));
        while (!t.isEmpty() && (t.front() == QLatin1Char('\n') || t.front() == QLatin1Char('\r')))
            t.remove(0, 1);
        if (newLine) {
            if (!first)
                out += QLatin1Char('\n');
            out += QStringLiteral("[%1]").arg(lrcTime(entry.time));
        }
        out += QStringLiteral("<%1>%2").arg(lrcTime(entry.time), t);
        first = false;
    }
    out += QLatin1Char('\n');
    return out;
}

QString syltFromId3(const TagLib::ID3v2::Tag *tag)
{
    if (!tag)
        return {};
    const auto &frames = tag->frameListMap()["SYLT"];
    for (auto *f : frames) {
        const auto *sylt = dynamic_cast<const TagLib::ID3v2::SynchronizedLyricsFrame *>(f);
        if (!sylt || sylt->type() != TagLib::ID3v2::SynchronizedLyricsFrame::Lyrics)
            continue;
        if (sylt->timestampFormat() != TagLib::ID3v2::SynchronizedLyricsFrame::AbsoluteMilliseconds)
            continue; // MPEG-frame timestamps would need the frame rate; rare in practice
        const QString lrc = syltToLrc(sylt);
        if (!lrc.isEmpty())
            return lrc;
    }
    return {};
}

} // namespace

namespace TagReader {

QString readLyrics(const QString &path)
{
    const QByteArray encoded = QFile::encodeName(path);
    TagLib::FileRef ref(encoded.constData(), false);
    if (ref.isNull())
        return {};

    // 1. ID3v2 SYLT (synced).
    TagLib::File *file = ref.file();
    QString sylt;
    if (auto *mpeg = dynamic_cast<TagLib::MPEG::File *>(file); mpeg && mpeg->hasID3v2Tag())
        sylt = syltFromId3(mpeg->ID3v2Tag());
    else if (auto *wav = dynamic_cast<TagLib::RIFF::WAV::File *>(file); wav && wav->hasID3v2Tag())
        sylt = syltFromId3(wav->ID3v2Tag());
    else if (auto *aiff = dynamic_cast<TagLib::RIFF::AIFF::File *>(file); aiff && aiff->hasID3v2Tag())
        sylt = syltFromId3(aiff->tag());
    if (!sylt.isEmpty())
        return sylt;

    // 2. Text properties. Embedded text is often LRC: prefer one with timestamps.
    const TagLib::PropertyMap props = ref.properties();
    QString firstPlain;
    for (const auto &[key, values] : props) {
        const QString k = toQString(key).toUpper();
        if (k != u"LYRICS" && k != u"UNSYNCEDLYRICS" && !k.startsWith(u"LYRICS:"))
            continue;
        for (const auto &v : values) {
            const QString text = toQString(v);
            if (text.trimmed().isEmpty())
                continue;
            if (LrcParser::looksSynced(text))
                return text;
            if (firstPlain.isEmpty())
                firstPlain = text;
        }
    }
    return firstPlain;
}

QByteArray readCover(const QString &path)
{
    const QByteArray encoded = QFile::encodeName(path);
    TagLib::FileRef ref(encoded.constData(), false);
    if (ref.isNull())
        return {};

    const auto pictures = ref.complexProperties("PICTURE");
    QByteArray fallback;
    for (const auto &pic : pictures) {
        const TagLib::ByteVector data = pic.value("data").toByteVector();
        if (data.isEmpty())
            continue;
        const QByteArray bytes(data.data(), qsizetype(data.size()));
        if (toQString(pic.value("pictureType").toString()) == u"Front Cover")
            return bytes;
        if (fallback.isEmpty())
            fallback = bytes;
    }
    return fallback;
}

} // namespace TagReader
