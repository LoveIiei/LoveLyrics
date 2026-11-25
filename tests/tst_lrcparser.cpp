// SPDX-License-Identifier: AGPL-3.0-or-later
#include "lyrics/LrcParser.h"
#include "lyrics/TextDecoding.h"

#include <QFile>
#include <QtTest>

class TestLrcParser : public QObject
{
    Q_OBJECT

    static LyricsDocument parseFile(const QString &name)
    {
        QFile f(QStringLiteral(TEST_DATA_DIR "/") + name);
        if (!f.open(QIODevice::ReadOnly))
            qFatal("missing test data %s", qPrintable(name));
        return LrcParser::parse(TextDecoding::decode(f.readAll()));
    }

    static QList<LyricLine> sungLines(const LyricsDocument &doc)
    {
        QList<LyricLine> out;
        for (const auto &l : doc.lines)
            if (!l.isInstrumental)
                out.append(l);
        return out;
    }

private slots:
    void timestamps_data()
    {
        QTest::addColumn<QString>("tag");
        QTest::addColumn<qint64>("ms");
        QTest::newRow("mm:ss.xx") << "01:02.34" << qint64(62340);
        QTest::newRow("mm:ss.xxx") << "01:02.345" << qint64(62345);
        QTest::newRow("mm:ss.x") << "01:02.3" << qint64(62300);
        QTest::newRow("mm:ss") << "01:02" << qint64(62000);
        QTest::newRow("mm:ss:xx") << "01:02:34" << qint64(62340);
        QTest::newRow("h:mm:ss.xx") << "1:02:03.50" << qint64(3723500);
        QTest::newRow("long minutes") << "120:00.00" << qint64(7200000);
        QTest::newRow("spaces") << " 00:01.00 " << qint64(1000);
    }
    void timestamps()
    {
        QFETCH(QString, tag);
        QFETCH(qint64, ms);
        const auto t = LrcParser::parseTimestamp(tag);
        QVERIFY(t.has_value());
        QCOMPARE(*t, ms);
    }

    void invalidTimestamps()
    {
        QVERIFY(!LrcParser::parseTimestamp(u"ar:Someone"));
        QVERIFY(!LrcParser::parseTimestamp(u"ab:cd"));
        QVERIFY(!LrcParser::parseTimestamp(u""));
        QVERIFY(!LrcParser::parseTimestamp(u"00:01.0000"));
    }

    void basicLinesAndEnds()
    {
        const auto doc = LrcParser::parse(QStringLiteral("[00:03.00]second\n[00:01.00]first\n[00:05.50]third\n"));
        QVERIFY(doc.synced);
        QCOMPARE(doc.lines.size(), 3);
        QCOMPARE(doc.lines[0].text, u"first");
        QCOMPARE(doc.lines[0].startMs, 1000);
        QCOMPARE(doc.lines[0].endMs, 3000);
        QCOMPARE(doc.lines[1].endMs, 5500);
        QVERIFY(doc.lines[2].endMs > 5500 + 2000); // last line: a few seconds
        QVERIFY(doc.lines[2].endMs <= 5500 + 8000);
        QVERIFY(!doc.hasWordTiming);
        // Words are estimated so styles always have something to animate.
        QCOMPARE(doc.lines[0].words.size(), 1);
        QVERIFY(doc.lines[0].wordsEstimated);
    }

    void multipleTimestamps()
    {
        const auto doc = LrcParser::parse(QStringLiteral("[00:12.00][01:30.00]chorus\n[00:20.00]verse\n"));
        const auto lines = sungLines(doc);
        QCOMPARE(lines.size(), 3);
        QCOMPARE(lines[0].text, u"chorus");
        QCOMPARE(lines[0].startMs, 12000);
        QCOMPARE(lines[1].text, u"verse");
        QCOMPARE(lines[2].text, u"chorus");
        QCOMPARE(lines[2].startMs, 90000);
    }

    void metadataAndOffset()
    {
        const auto doc = LrcParser::parse(QStringLiteral(
            "[ti:Title]\n[ar:Artist]\n[al:Album]\n[by:me]\n[offset:+500]\n[length:03:00]\n[00:02.00]hello\n"));
        QCOMPARE(doc.title, u"Title");
        QCOMPARE(doc.artist, u"Artist");
        QCOMPARE(doc.album, u"Album");
        QCOMPARE(doc.offsetMs, 500);
        QCOMPARE(doc.lines.size(), 1);
        QCOMPARE(doc.lines[0].startMs, 1500); // positive offset: lyrics appear sooner

        const auto neg = LrcParser::parse(QStringLiteral("[offset:-250]\n[00:02.00]hello\n"));
        QCOMPARE(neg.lines[0].startMs, 2250);
    }

    void translations()
    {
        const auto doc = LrcParser::parse(QStringLiteral("[00:01.00]Hello\n[00:01.00]你好\n"
                                                         "[00:04.00]World\n[00:04.00]世界\n"));
        QVERIFY(doc.hasTranslation);
        QCOMPARE(doc.lines.size(), 2);
        QCOMPARE(doc.lines[0].text, u"Hello");
        QCOMPARE(doc.lines[0].translation, u"你好");
        QCOMPARE(doc.lines[1].translation, u"世界");
    }

    void blankSharingTimestampDoesNotStealOriginal()
    {
        // A blank timed line sharing a timestamp must not push the original into the translation.
        const auto doc = LrcParser::parse(QStringLiteral("[00:01.00]\n[00:01.00]Original\n[00:01.00]译文\n[00:05.00]Next\n"));
        const auto lines = sungLines(doc);
        QCOMPARE(lines[0].text, u"Original");
        QCOMPARE(lines[0].translation, u"译文");
    }

    void romanization()
    {
        const auto doc = LrcParser::parse(
            QStringLiteral("[00:01.00]桜が咲いた\n[00:01.00]sakura ga saita\n[00:01.00]樱花开了\n"));
        QCOMPARE(doc.lines[0].text, u"桜が咲いた");
        QCOMPARE(doc.lines[0].romanization, u"sakura ga saita");
        QCOMPARE(doc.lines[0].translation, u"樱花开了");

        // A lone Latin line under CJK is a translation, not romanization.
        const auto doc2 = LrcParser::parse(QStringLiteral("[00:01.00]你好\n[00:01.00]Hello\n"));
        QCOMPARE(doc2.lines[0].translation, u"Hello");
        QVERIFY(doc2.lines[0].romanization.isEmpty());
    }

    void enhancedWords()
    {
        const auto doc =
            LrcParser::parse(QStringLiteral("[00:12.00]<00:12.00>Hello <00:12.50>world<00:13.20>\n[00:20.00]next\n"));
        QVERIFY(doc.hasWordTiming);
        const auto &l = sungLines(doc)[0];
        QCOMPARE(l.text, u"Hello world");
        QVERIFY(!l.wordsEstimated);
        QCOMPARE(l.words.size(), 2);
        QCOMPARE(l.words[0].text, u"Hello ");
        QCOMPARE(l.words[0].startMs, 12000);
        QCOMPARE(l.words[0].endMs, 12500);
        QCOMPARE(l.words[1].text, u"world");
        QCOMPARE(l.words[1].startMs, 12500);
        QCOMPARE(l.words[1].endMs, 13200);
        QCOMPARE(l.endMs, 13200);
    }

    void enhancedA2()
    {
        const auto doc = LrcParser::parse(QStringLiteral("[00:12.00]Hello<00:12.50> world<00:13.00> again\n[00:15.00]x\n"));
        QVERIFY(doc.lines[0].isInstrumental); // intro before 12 s
        const auto &l = doc.lines[1];
        QCOMPARE(l.text, u"Hello world again");
        QCOMPARE(l.words.size(), 3);
        QCOMPARE(l.words[0].text, u"Hello ");
        QCOMPARE(l.words[0].startMs, 12000);
        QCOMPARE(l.words[0].endMs, 12500);
        QCOMPARE(l.words[1].text, u"world ");
        QCOMPARE(l.words[2].text, u"again");
        QCOMPARE(l.words[2].startMs, 13000);
        QCOMPARE(l.words[2].endMs, 15000); // open last word runs to the next line
    }

    void enhancedWithMultipleTimestampsShiftsWords()
    {
        const auto doc = LrcParser::parse(QStringLiteral("[00:10.00][01:10.00]<00:10.00>la <00:10.50>la<00:11.00>\n"));
        const auto lines = sungLines(doc);
        QCOMPARE(lines.size(), 2);
        QCOMPARE(lines[1].words[1].startMs, 70500);
        QCOMPARE(lines[1].words[1].endMs, 71000);
    }

    void instrumentalBreaks()
    {
        const auto doc = LrcParser::parse(QStringLiteral("[00:10.00]Hi\n[00:12.00]\n[00:40.00]Back\n"));
        QVERIFY(doc.lines.size() >= 4);
        QVERIFY(doc.lines[0].isInstrumental); // intro 0..10 s
        QCOMPARE(doc.lines[0].startMs, 0);
        QCOMPARE(doc.lines[0].endMs, 10000);
        QCOMPARE(doc.lines[1].text, u"Hi");
        QCOMPARE(doc.lines[1].endMs, 12000); // blank line marks the end
        QVERIFY(doc.lines[2].isInstrumental);
        QCOMPARE(doc.lines[2].startMs, 12000);
        QCOMPARE(doc.lines[2].endMs, 40000);
        QCOMPARE(doc.lines[3].text, u"Back");
    }

    void longGapWithoutBlankBecomesInstrumental()
    {
        const auto doc = LrcParser::parse(QStringLiteral("[00:00.50]Short\n[00:30.00]Later\n"));
        QCOMPARE(doc.lines.size(), 3);
        QVERIFY(doc.lines[0].endMs < 10000);
        QVERIFY(doc.lines[1].isInstrumental);
    }

    void plainLyrics()
    {
        const auto doc = LrcParser::parse(QStringLiteral("[ti:Song]\n\nLine one\nLine two\n\n\nLine three\n\n"));
        QVERIFY(!doc.synced);
        QCOMPARE(doc.title, u"Song");
        QCOMPARE(doc.lines.size(), 4);
        QCOMPARE(doc.lines[0].text, u"Line one");
        QCOMPARE(doc.lines[2].text, u""); // paragraph break kept once
        QCOMPARE(doc.lines[3].text, u"Line three");
    }

    void brokenLinesDoNotCrash()
    {
        const auto doc = LrcParser::parse(QStringLiteral(
            "[00:xx.00]bad\n[garbage\n]]\n[00:01.00]good\n<00:02.00>\n[00:02.00][Chorus] text\n[99999999999:00.00]huge\n"));
        QVERIFY(doc.synced);
        const auto lines = sungLines(doc);
        QVERIFY(lines.size() >= 2);
        QCOMPARE(lines[0].text, u"good");
        QCOMPARE(lines[1].text, u"[Chorus] text");
    }

    void carriageReturns()
    {
        const auto doc = LrcParser::parse(QStringLiteral("[00:01.00]a\r\n[00:02.00]b\r\r\n[00:03.00]c\r"));
        QCOMPARE(sungLines(doc).size(), 3);
        QCOMPARE(doc.lines[1].text, u"b");
    }

    void estimateWordsCjk()
    {
        const auto words = LrcParser::estimateWords(QStringLiteral("你好，世界"), 1000, 3000);
        QCOMPARE(words.size(), 5);
        QCOMPARE(words[0].text, u"你");
        QCOMPARE(words[2].text, u"，");
        QCOMPARE(words[0].startMs, 1000);
        for (const auto &w : words) {
            QVERIFY(w.startMs >= 1000);
            QVERIFY(w.endMs <= 3000);
        }
        // Punctuation gets (almost) no time.
        QVERIFY(words[3].startMs - words[2].startMs <= 50);
    }

    void estimateWordsLatin()
    {
        const auto words = LrcParser::estimateWords(QStringLiteral("we are neon"), 0, 3000);
        QCOMPARE(words.size(), 3);
        QCOMPARE(words[0].text, u"we ");
        QCOMPARE(words[2].text, u"neon");
        QVERIFY(words[1].startMs > words[0].startMs);
        QVERIFY(words[2].endMs <= 3000);
        // Longer words take longer.
        QVERIFY(words[2].endMs - words[2].startMs > words[0].endMs - words[0].startMs);
    }

    void demoFile()
    {
        const auto doc = parseFile(QStringLiteral("library/01 Neon Rain.lrc"));
        QVERIFY(doc.synced);
        QVERIFY(doc.hasTranslation);
        QCOMPARE(doc.title, u"Neon Rain");
        QVERIFY(doc.lines.first().isInstrumental); // 6 s intro
        int neon = 0;
        for (const auto &l : doc.lines)
            if (l.text == u"We are neon in the rain")
                ++neon;
        QCOMPARE(neon, 2);
        for (qsizetype i = 1; i < doc.lines.size(); ++i)
            QVERIFY(doc.lines[i].startMs >= doc.lines[i - 1].startMs);

        const auto karaoke = parseFile(QStringLiteral("library/02 Paper Moon.lrc"));
        QVERIFY(karaoke.hasWordTiming);
        QCOMPARE(sungLines(karaoke)[3].words.size(), 6); // 纸月亮在海上
    }
};

QTEST_GUILESS_MAIN(TestLrcParser)
#include "tst_lrcparser.moc"
