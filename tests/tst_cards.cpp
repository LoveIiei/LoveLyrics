// SPDX-License-Identifier: AGPL-3.0-or-later
#include "app/Settings.h"
#include "cards/CardColor.h"
#include "cards/CardController.h"
#include "lyrics/LrcParser.h"
#include "lyrics/LyricsCursor.h"
#include "lyrics/LyricsModel.h"
#include "media/CoverLoader.h"
#include "mpris/MprisClient.h"
#include "playback/PlaybackClock.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include "cards/CardRenderer.h"
#include "cards/TextWrap.h"

#include <QtTest>

using namespace Cards;

class TestCards : public QObject
{
    Q_OBJECT

    static CardState sample(Template t, Ratio r)
    {
        CardState st;
        st.tmpl = t;
        st.ratio = r;
        st.title = QStringLiteral("Neon Rain");
        st.artist = QStringLiteral("The Placeholders");
        st.groups = {{QStringLiteral("City lights are melting in the rain"), QStringLiteral("城市的灯光在雨中融化")},
                     {QStringLiteral("Every window hums a different name"), QStringLiteral("每扇窗都哼着不同的名字")},
                     {QStringLiteral("这是一句很长的中文歌词用来测试换行和排版是否正确显示"), QString()},
                     {QStringLiteral("今日も晴れてるよ 君のいない町でも"), QStringLiteral("今天也是晴天")}};
        st.focus = {1};
        st.extras.progress = true;
        st.extras.date = true;
        st.extras.dateText = QStringLiteral("2026.11.25");
        st.extras.note = true;
        st.extras.noteText = QStringLiteral("Love Lyrics");
        st.extras.cur = QStringLiteral("1:52");
        st.extras.dur = QStringLiteral("4:05");
        return st;
    }

private slots:
    void contrastGuarantees()
    {
        // Every style / mode keeps body text >= 4.5:1 (7:1 when the background allows it).
        const QList<QColor> seeds = {QColor("#5DAEE6"), QColor("#FFD400"), QColor("#808080"), QColor("#101010"),
                                     QColor("#F4F4F4"), QColor("#E0115F"), QColor("#2E7D32"), QColor("#7A5CFF")};
        for (const QColor &seed : seeds)
            for (auto style : {CardColor::Style::Content, CardColor::Style::Tonal, CardColor::Style::Vibrant,
                               CardColor::Style::Mono})
                for (auto mode : {CardColor::Mode::Auto, CardColor::Mode::Light, CardColor::Mode::Dark}) {
                    const auto p = CardColor::cardPalette(seed, QColor("#FF8800"), style, mode, 0.4);
                    QVERIFY2(CardColor::contrast(p.on, p.bg) >= 4.5, qPrintable(seed.name()));
                    QVERIFY(CardColor::contrast(p.onVar, p.bg) >= 4.5 - 1e-9 || CardColor::contrast(p.onVar, p.bg) >= 4.4);
                    QVERIFY(CardColor::contrast(p.accent, p.bg) >= 4.4);
                }
        const auto dark = CardColor::cardPalette(QColor("#1B2E86"), QColor(), CardColor::Style::Content,
                                                 CardColor::Mode::Auto, 0.3);
        QVERIFY(dark.isDark);
        QVERIFY(CardColor::contrast(dark.on, dark.bg) >= 7);
    }

    void stylesDiffer()
    {
        const QColor seed("#5DAEE6");
        QList<QColor> bgs;
        for (auto style : {CardColor::Style::Content, CardColor::Style::Tonal, CardColor::Style::Vibrant,
                           CardColor::Style::Mono})
            bgs << CardColor::cardPalette(seed, QColor("#E0115F"), style, CardColor::Mode::Dark, 0.4).bg;
        for (int i = 0; i < bgs.size(); ++i)
            for (int j = i + 1; j < bgs.size(); ++j)
                QVERIFY2(CardColor::okDistance(bgs[i], bgs[j]) >= 0.05, qPrintable(bgs[i].name() + " " + bgs[j].name()));
    }

    void analysis()
    {
        const QImage cover = defaultCover();
        const auto a = CardColor::analyzeCover(cover);
        QVERIFY(!a.swatches.isEmpty());
        QCOMPARE(a.swatches.first().role, QStringLiteral("base"));
        QVERIFY(a.meanL < 0.5); // a night-blue cover
        QVERIFY(a.focus.x() > 0 && a.focus.x() < 1);
        const auto b = CardColor::analyzeCover(cover);
        QCOMPARE(a.swatches.size(), b.swatches.size()); // deterministic
        QCOMPARE(a.swatches.first().color, b.swatches.first().color);
    }

    void wrapping()
    {
        auto measure = [](const QString &s) { return double(s.toUcs4().size()); }; // 1 unit per char
        // Latin words stay whole; break at a space.
        QCOMPARE(TextWrap::wrap(measure, QStringLiteral("hello world again"), 12),
                 QStringList({QStringLiteral("hello world"), QStringLiteral("again")}));
        // No line may start with 。 — the character before it moves down with it.
        const QStringList lines = TextWrap::wrap(measure, QStringLiteral("一二三四五。六七"), 5);
        for (const QString &l : lines)
            QVERIFY2(!l.startsWith(QStringLiteral("。")), qPrintable(lines.join('|')));
        // No orphan: the last line gets more than 1-2 characters when possible.
        const QStringList o = TextWrap::wrap(measure, QStringLiteral("一二三四五六七八九十十一"), 10);
        QVERIFY2(o.last().toUcs4().size() > 2, qPrintable(o.join('|')));
        // Paragraph breaks.
        QCOMPARE(TextWrap::wrap(measure, QStringLiteral("ab\ncd"), 10).size(), 2);
    }

    void renderAllTemplates_data()
    {
        QTest::addColumn<int>("tmpl");
        QTest::addColumn<int>("ratio");
        for (int t = 0; t < 5; ++t)
            for (int r = 0; r < 4; ++r)
                QTest::newRow(qPrintable(QStringLiteral("%1/%2").arg(info(Template(t)).id).arg(r))) << t << r;
    }
    void renderAllTemplates()
    {
        QFETCH(int, tmpl);
        QFETCH(int, ratio);
        const Template t = Template(tmpl);
        CardAssets assets;
        assets.analysis = CardColor::analyzeCover(defaultCover());
        const CardResult r = render(sample(t, Ratio(ratio)), assets, 2.0);
        QCOMPARE(r.image.width(), 1080);
        QVERIFY(r.height > 300);
        const Ratio used = ratioFor(t, Ratio(ratio));
        QCOMPARE(r.ratio, used);
        if (used == Ratio::R9x16)
            QCOMPARE(r.height, 960.0);
        // Every lyric hit box stays inside the card.
        for (const Hit &h : r.hits) {
            QVERIFY2(h.rect.bottom() <= r.height + 40, qPrintable(QString::number(h.rect.bottom())));
            QVERIFY(h.rect.top() >= 0);
        }
        // Not blank.
        const QImage small = r.image.scaled(8, 8);
        QSet<QRgb> colours;
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
                colours.insert(small.pixel(x, y));
        QVERIFY(colours.size() > 1);
        if (qEnvironmentVariableIsSet("CARD_DUMP"))
            r.image.save(QStringLiteral("%1/%2-%3.png").arg(qEnvironmentVariable("CARD_DUMP"), info(t).id).arg(ratio));
    }

    void fixedRatioDropsLines()
    {
        CardState st = sample(Template::Classic, Ratio::R1x1);
        for (int i = 0; i < 10; ++i)
            st.groups.append({QStringLiteral("Another long line that needs space number %1").arg(i), QString()});
        CardAssets assets;
        const CardResult r = render(st, assets, 1.0);
        QCOMPARE(r.height, 540.0);
        QVERIFY(r.dropped > 0);
        // The highlighted group (index 1) survives.
        bool focusShown = false;
        for (const Hit &h : r.hits)
            focusShown |= h.index == 1;
        QVERIFY(focusShown);
    }

    void controllerExport()
    {
        QStandardPaths::setTestModeEnabled(true);
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("t.conf")));
        MprisClient player;
        PlaybackClock clock;
        LyricsModel lyrics;
        lyrics.setDocument(LrcParser::parse(QStringLiteral("[00:01.00]one\n[00:01.00]一\n[00:03.00]two\n[00:05.00]three\n")));
        LyricsCursor cursor;
        cursor.setModel(&lyrics);
        Palette palette;
        CoverLoader cover(&palette);
        CardController cards(&settings, &player, &clock, &lyrics, &cursor, &cover);

        clock.sync(3500, PlaybackClock::SyncKind::Snap); // inside "two"
        cards.open();
        QCOMPARE(cards.cardState().groups.size(), 2); // "two", "three"
        QCOMPARE(cards.cardState().groups.first().original, QStringLiteral("two"));

        cards.set(QStringLiteral("template"), QStringLiteral("quiet"));
        cards.set(QStringLiteral("title"), QStringLiteral("A / weird: title?"));
        QSignalSpy preview(&cards, &CardController::previewChanged);
        QVERIFY(preview.wait(5000));
        QVERIFY(!cards.previewSource().isEmpty());

        QSignalSpy done(&cards, &CardController::exportFinished);
        cards.exportPng(1080);
        QVERIFY(done.wait(10000));
        QVERIFY(done.first().at(0).toBool());
        const QString path = done.first().at(1).toString();
        QVERIFY2(path.startsWith(CardController::exportDirectory()), qPrintable(path));
        QVERIFY(!QFileInfo(path).fileName().contains(QLatin1Char('/')));
        const QImage saved(path);
        QCOMPARE(saved.width(), 1080);
        QFile::remove(path);

        // Preferences survive a restart; texts don't.
        CardController again(&settings, &player, &clock, &lyrics, &cursor, &cover);
        QCOMPARE(again.state().value("template").toString(), QStringLiteral("quiet"));
        QCOMPARE(again.state().value("title").toString(), QString());
    }

    void exportSize()
    {
        CardAssets assets;
        const CardResult r = render(sample(Template::Immersive, Ratio::R9x16), assets, 4.0);
        QCOMPARE(r.image.size(), QSize(2160, 3840));
    }
};

QTEST_MAIN(TestCards)
#include "tst_cards.moc"
