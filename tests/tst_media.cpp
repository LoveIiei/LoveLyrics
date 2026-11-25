// SPDX-License-Identifier: AGPL-3.0-or-later
// TrackResolver, TagReader, LyricsLoader, CoverLoader, PaletteExtractor.
#include "lyrics/LyricsLoader.h"
#include "media/CoverLoader.h"
#include "media/PaletteExtractor.h"
#include "media/TagReader.h"
#include "media/TrackResolver.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <attachedpictureframe.h>
#include <id3v2tag.h>
#include <synchronizedlyricsframe.h>
#include <unsynchronizedlyricsframe.h>
#include <wavfile.h>

namespace {

QString libraryPath(const QString &name)
{
    return QStringLiteral(TEST_DATA_DIR "/library/") + name;
}

QString fileUrl(const QString &path)
{
    return QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded);
}

void touch(const QString &path, const QByteArray &content = {})
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
}

// A tiny valid PCM WAV (0.1 s of silence) that TagLib can tag with ID3v2.
void writeWav(const QString &path)
{
    const quint32 rate = 8000, samples = 800;
    QByteArray data;
    QDataStream s(&data, QIODevice::WriteOnly);
    s.setByteOrder(QDataStream::LittleEndian);
    s.writeRawData("RIFF", 4);
    s << quint32(36 + samples * 2);
    s.writeRawData("WAVEfmt ", 8);
    s << quint32(16) << quint16(1) << quint16(1) << rate << quint32(rate * 2) << quint16(2) << quint16(16);
    s.writeRawData("data", 4);
    s << quint32(samples * 2);
    data.append(QByteArray(samples * 2, '\0'));
    touch(path, data);
}

} // namespace

class TestMedia : public QObject
{
    Q_OBJECT

private slots:
    void localPath()
    {
        QCOMPARE(TrackResolver::localPath(QStringLiteral("file:///music/A%20B/C%23D%20%E7%88%B1.flac")),
                 QStringLiteral("/music/A B/C#D 爱.flac"));
        QCOMPARE(TrackResolver::localPath(QStringLiteral("/plain/path.mp3")), QStringLiteral("/plain/path.mp3"));
        QCOMPARE(TrackResolver::localPath(QStringLiteral("http://example.org/stream")), QString());
        QCOMPARE(TrackResolver::localPath(QString()), QString());
    }

    void sidecarUnicodeAndCase()
    {
        QTemporaryDir dir;
        const QString audio = dir.filePath(QStringLiteral("01 My Sóng 爱.flac"));
        touch(audio);
        touch(dir.filePath(QStringLiteral("01 my sÓng 爱.LRC")));
        touch(dir.filePath(QStringLiteral("01 Other.lrc")));
        const QString found = TrackResolver::findSidecarLyrics(TrackResolver::localPath(fileUrl(audio)));
        QCOMPARE(QFileInfo(found).fileName(), QStringLiteral("01 my sÓng 爱.LRC"));
    }

    void sidecarFullName()
    {
        QTemporaryDir dir;
        const QString audio = dir.filePath(QStringLiteral("Song.flac"));
        touch(dir.filePath(QStringLiteral("Song.flac.lrc")));
        QCOMPARE(QFileInfo(TrackResolver::findSidecarLyrics(audio)).fileName(), QStringLiteral("Song.flac.lrc"));
        // The plain stem wins when both exist.
        touch(dir.filePath(QStringLiteral("Song.lrc")));
        QCOMPARE(QFileInfo(TrackResolver::findSidecarLyrics(audio)).fileName(), QStringLiteral("Song.lrc"));
    }

    void folderCover()
    {
        QTemporaryDir dir;
        touch(dir.filePath(QStringLiteral("back.jpg")));
        touch(dir.filePath(QStringLiteral("Folder.JPG")));
        QCOMPARE(QFileInfo(TrackResolver::findFolderCover(dir.filePath(QStringLiteral("x.flac")))).fileName(),
                 QStringLiteral("Folder.JPG"));
        touch(dir.filePath(QStringLiteral("cover.png")));
        QCOMPARE(QFileInfo(TrackResolver::findFolderCover(dir.filePath(QStringLiteral("x.flac")))).fileName(),
                 QStringLiteral("cover.png"));
    }

    void loaderSidecar()
    {
        TrackInfo t;
        t.url = fileUrl(libraryPath(QStringLiteral("01 Neon Rain.flac")));
        const auto doc = LyricsLoader::loadNow(t, LyricsLoader::DefaultSourceOrder);
        QCOMPARE(doc.source, u"sidecar");
        QVERIFY(doc.synced);
        QVERIFY(doc.hasTranslation);

        t.url = fileUrl(libraryPath(QStringLiteral("03 Quiet Hours.flac")));
        const auto plain = LyricsLoader::loadNow(t, LyricsLoader::DefaultSourceOrder);
        QVERIFY(!plain.synced);
        QVERIFY(!plain.isEmpty());

        t.url = fileUrl(libraryPath(QStringLiteral("04 Instrumental.flac")));
        QVERIFY(LyricsLoader::loadNow(t, LyricsLoader::DefaultSourceOrder).isEmpty());
    }

    void loaderMprisAndOrder()
    {
        TrackInfo t;
        t.url = fileUrl(libraryPath(QStringLiteral("01 Neon Rain.flac")));
        t.lyricsText = QStringLiteral("[00:01.00]from mpris");
        QCOMPARE(LyricsLoader::loadNow(t, {QStringLiteral("mpris"), QStringLiteral("sidecar")}).source, u"mpris");
        QCOMPARE(LyricsLoader::loadNow(t, LyricsLoader::DefaultSourceOrder).source, u"sidecar");

        t.url = QStringLiteral("http://radio/stream");
        QCOMPARE(LyricsLoader::loadNow(t, LyricsLoader::DefaultSourceOrder).source, u"mpris");
    }

    void loaderAsyncDropsStale()
    {
        LyricsLoader loader;
        QSignalSpy spy(&loader, &LyricsLoader::loaded);
        TrackInfo a;
        a.url = fileUrl(libraryPath(QStringLiteral("01 Neon Rain.flac")));
        TrackInfo b;
        b.url = fileUrl(libraryPath(QStringLiteral("02 Paper Moon.flac")));
        loader.load(a);
        loader.load(b); // supersedes a
        QVERIFY(spy.wait(5000));
        QTest::qWait(200);
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.first().first().value<LyricsDocument>().hasWordTiming); // Paper Moon
    }

    void embeddedSyltAndUslt()
    {
        QTemporaryDir dir;
        const QString wav = dir.filePath(QStringLiteral("tagged.wav"));
        writeWav(wav);
        {
            TagLib::RIFF::WAV::File f(QFile::encodeName(wav).constData());
            QVERIFY(f.isValid());
            auto *uslt = new TagLib::ID3v2::UnsynchronizedLyricsFrame(TagLib::String::UTF8);
            uslt->setText(TagLib::String("plain words only", TagLib::String::UTF8));
            f.ID3v2Tag()->addFrame(uslt);
            QVERIFY(f.save());
        }
        QCOMPARE(TagReader::readLyrics(wav), QStringLiteral("plain words only"));

        {
            TagLib::RIFF::WAV::File f(QFile::encodeName(wav).constData());
            auto *sylt = new TagLib::ID3v2::SynchronizedLyricsFrame(TagLib::String::UTF8);
            sylt->setTimestampFormat(TagLib::ID3v2::SynchronizedLyricsFrame::AbsoluteMilliseconds);
            sylt->setType(TagLib::ID3v2::SynchronizedLyricsFrame::Lyrics);
            sylt->setSynchedText({{1500, TagLib::String("第一行", TagLib::String::UTF8)},
                                  {62250, TagLib::String("second", TagLib::String::UTF8)}});
            f.ID3v2Tag()->addFrame(sylt);
            QVERIFY(f.save());
        }
        const QString lrc = TagReader::readLyrics(wav);
        QVERIFY2(lrc.contains(QStringLiteral("[00:01.500]第一行")), qPrintable(lrc));
        QVERIFY2(lrc.contains(QStringLiteral("[01:02.250]second")), qPrintable(lrc));

        TrackInfo t;
        t.url = fileUrl(wav);
        const auto doc = LyricsLoader::loadNow(t, LyricsLoader::DefaultSourceOrder);
        QCOMPARE(doc.source, u"embedded");
        QVERIFY(doc.synced);
    }

    void embeddedCover()
    {
        QTemporaryDir dir;
        const QString wav = dir.filePath(QStringLiteral("cover.wav"));
        writeWav(wav);
        QFile png(libraryPath(QStringLiteral("cover.png")));
        QVERIFY(png.open(QIODevice::ReadOnly));
        const QByteArray bytes = png.readAll();
        {
            TagLib::RIFF::WAV::File f(QFile::encodeName(wav).constData());
            auto *pic = new TagLib::ID3v2::AttachedPictureFrame;
            pic->setMimeType("image/png");
            pic->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
            pic->setPicture(TagLib::ByteVector(bytes.constData(), uint(bytes.size())));
            f.ID3v2Tag()->addFrame(pic);
            QVERIFY(f.save());
        }
        QCOMPARE(TagReader::readCover(wav), bytes);

        TrackInfo t;
        t.url = fileUrl(wav);
        const auto r = CoverLoader::loadNow(t);
        QCOMPARE(r.origin, u"embedded");
        QCOMPARE(r.full.size(), QSize(256, 256));
    }

    void coverSources()
    {
        TrackInfo t;
        t.url = fileUrl(libraryPath(QStringLiteral("01 Neon Rain.flac")));
        auto r = CoverLoader::loadNow(t);
        QCOMPARE(r.origin, u"folder");
        QCOMPARE(r.blurred.size(), QSize(CoverLoader::BlurSize, CoverLoader::BlurSize));

        t.artUrl = fileUrl(libraryPath(QStringLiteral("cover.png")));
        r = CoverLoader::loadNow(t);
        QCOMPARE(r.origin, u"mpris");

        TrackInfo none;
        none.url = QStringLiteral("file:///nonexistent/dir/x.flac");
        QVERIFY(CoverLoader::loadNow(none).full.isNull());
    }

    void palette()
    {
        QImage red(64, 64, QImage::Format_ARGB32);
        red.fill(QColor(200, 30, 40));
        const auto p = PaletteExtractor::extract(red);
        QVERIFY(p.vibrant.red() > 150 && p.vibrant.green() < 80);
        QVERIFY(p.dark.lightnessF() < 0.15);
        QVERIFY(PaletteExtractor::contrastRatio(p.text, p.dark) >= 7.0);
        QVERIFY(qAbs(p.dark.hslHueF() - p.vibrant.hslHueF()) < 0.05 || p.dark.hslHueF() > 0.95);

        QImage gray(32, 32, QImage::Format_ARGB32);
        gray.fill(QColor(128, 128, 128));
        const auto g = PaletteExtractor::extract(gray);
        QVERIFY(PaletteExtractor::contrastRatio(g.text, g.dark) >= 7.0);

        QCOMPARE(PaletteExtractor::extract(QImage()), PaletteColors::defaults());

        QImage cover(libraryPath(QStringLiteral("cover.png")));
        QCOMPARE(PaletteExtractor::extract(cover), PaletteExtractor::extract(cover)); // deterministic
    }
};

QTEST_GUILESS_MAIN(TestMedia)
#include "tst_media.moc"
