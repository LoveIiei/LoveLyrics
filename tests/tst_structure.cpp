// SPDX-License-Identifier: AGPL-3.0-or-later
#include "lyrics/LrcParser.h"
#include "lyrics/SongStructure.h"

#include <QFile>
#include <QtTest>

class TestStructure : public QObject
{
    Q_OBJECT

    static LyricsDocument parsed(const QString &lrc)
    {
        LyricsDocument d = LrcParser::parse(lrc);
        SongStructure::analyze(d);
        return d;
    }

private slots:
    void chorusIsMostRepeated()
    {
        const auto d = parsed(QStringLiteral("[00:01.00]verse one\n[00:04.00]Hold me!\n[00:07.00]verse two\n"
                                             "[00:10.00]hold me\n[00:13.00]verse three\n[00:16.00]Hold me.\n"
                                             "[00:19.00]verse one\n"));
        QVERIFY(d.lines[1].isChorus);
        QVERIFY(d.lines[3].isChorus); // punctuation / case don't matter
        QVERIFY(!d.lines[0].isChorus);  // repeated twice, chorus three times
        const auto none = parsed(QStringLiteral("[00:01.00]a line\n[00:03.00]another\n"));
        QVERIFY(!none.lines[0].isChorus);
    }

    void sectionsSplitAtGaps()
    {
        QString lrc;
        // Two blocks of 4 lines 2 s apart, separated by a 5 s gap.
        for (int i = 0; i < 4; ++i)
            lrc += QStringLiteral("[00:%1.00]first block line %2 with words\n").arg(10 + i * 2, 2, 10, QLatin1Char('0')).arg(i);
        for (int i = 0; i < 4; ++i)
            lrc += QStringLiteral("[00:%1.00]second block line %2 with words\n").arg(23 + i * 2, 2, 10, QLatin1Char('0')).arg(i);
        const auto d = parsed(lrc);
        QList<int> sung;
        for (const auto &l : d.lines)
            if (!l.isInstrumental)
                sung << l.section;
        QCOMPARE(sung.first(), sung[3]);
        QVERIFY(sung[3] != sung[4]);
        QCOMPARE(d.sections.last().kind, QStringLiteral("outro"));
        QVERIFY(d.seed != 0);
        QCOMPARE(parsed(lrc).seed, d.seed); // stable
    }

    void oversizedSectionsSplit()
    {
        QString lrc;
        for (int i = 0; i < 14; ++i)
            lrc += QStringLiteral("[00:%1.00]line number %2 keeps going on\n").arg(10 + i * 2, 2, 10, QLatin1Char('0')).arg(i);
        const auto d = parsed(lrc);
        QVERIFY(d.sections.size() >= 3);
        for (const auto &s : d.sections)
            QVERIFY(s.lastLine - s.firstLine + 1 <= 6);
    }

    void unitsLatin()
    {
        const auto d = parsed(QStringLiteral("[00:01.00]We are neon, in the rain!\n[00:05.00]x\n"));
        const auto u = SongStructure::units(d.lines[0]);
        QStringList texts;
        for (const auto &x : u)
            texts << x.text;
        QCOMPARE(texts, QStringList({"We", "are", "neon,", "in", "the", "rain!"}));
        QVERIFY(u[0].spaceAfter);
        QVERIFY(!u.last().spaceAfter);
        for (int i = 1; i < u.size(); ++i)
            QVERIFY(u[i].startMs >= u[i - 1].startMs);
        QCOMPARE(u.first().startMs, 1000);
    }

    void unitsCjk()
    {
        const auto d = parsed(QStringLiteral("[00:01.00]城市的灯光在雨中融化，「好吗」\n[00:06.00]x\n"));
        const auto u = SongStructure::units(d.lines[0]);
        QString joined;
        for (const auto &x : u)
            joined += x.text;
        QCOMPARE(joined, QStringLiteral("城市的灯光在雨中融化，「好吗」"));
        QStringList parts;
        for (const auto &x : u)
            parts << x.text;
        qInfo("units: %s", qPrintable(parts.join(QLatin1Char('|'))));
        QVERIFY(u.size() >= 4);           // dictionary words, not one unit
        QVERIFY(u.size() < 12);           // and not one per character either
        for (const auto &x : u)
            QVERIFY2(!x.text.startsWith(QStringLiteral("，")), qPrintable(x.text)); // punctuation sticks to the previous unit
    }

    void unitsWordTimed()
    {
        const auto d = parsed(QStringLiteral("[00:12.00]<00:12.00>Hello <00:12.50>world<00:13.20>\n[00:20.00]x\n"));
        const auto &line = d.lines[1]; // [0] is the intro
        const auto u = SongStructure::units(line);
        QCOMPARE(u.size(), 2);
        QCOMPARE(u[1].startMs, 12500);
        QCOMPARE(u[1].endMs, 13200);
    }
};

QTEST_GUILESS_MAIN(TestStructure)
#include "tst_structure.moc"
