// SPDX-License-Identifier: AGPL-3.0-or-later
#include "lyrics/LyricsCursor.h"
#include "lyrics/LyricsModel.h"

#include <QSignalSpy>
#include <QtTest>

class TestCursor : public QObject
{
    Q_OBJECT

    static LyricsDocument doc()
    {
        LyricsDocument d;
        d.synced = true;
        auto line = [](qint64 s, qint64 e, const char *t) {
            LyricLine l;
            l.startMs = s;
            l.endMs = e;
            l.text = QString::fromUtf8(t);
            return l;
        };
        d.lines = {line(1000, 3000, "a"), line(3000, 5000, "b"), line(8000, 9000, "c")};
        return d;
    }

private slots:
    void indices_data()
    {
        QTest::addColumn<double>("pos");
        QTest::addColumn<int>("active");
        QTest::addColumn<int>("focus");
        QTest::addColumn<int>("next");
        QTest::newRow("before first") << 500.0 << -1 << -1 << 0;
        QTest::newRow("first start") << 1000.0 << 0 << 0 << 1;
        QTest::newRow("just before second") << 2999.9 << 0 << 0 << 1;
        QTest::newRow("second start") << 3000.0 << 1 << 1 << 2;
        QTest::newRow("gap") << 6000.0 << -1 << 1 << 2;
        QTest::newRow("last") << 8500.0 << 2 << 2 << -1;
        QTest::newRow("after last") << 9500.0 << -1 << 2 << -1;
    }
    void indices()
    {
        QFETCH(double, pos);
        QFETCH(int, active);
        QFETCH(int, focus);
        QFETCH(int, next);
        LyricsModel model;
        model.setDocument(doc());
        LyricsCursor cursor;
        cursor.setModel(&model);
        cursor.setPositionMs(pos);
        QCOMPARE(cursor.activeIndex(), active);
        QCOMPARE(cursor.focusIndex(), focus);
        QCOMPARE(cursor.nextIndex(), next);
        QCOMPARE(LyricsCursor::activeIndexAt(doc().lines, pos), active);
    }

    void progressAndSignals()
    {
        LyricsModel model;
        model.setDocument(doc());
        LyricsCursor cursor;
        cursor.setModel(&model);
        QSignalSpy activeSpy(&cursor, &LyricsCursor::activeIndexChanged);
        cursor.setPositionMs(2000);
        QCOMPARE(cursor.progress(), 0.5);
        QCOMPARE(cursor.timeToNextMs(), 1000.0);
        cursor.setPositionMs(2100);
        cursor.setPositionMs(2200);
        QCOMPARE(activeSpy.count(), 1); // only fires on change

        model.clear();
        QCOMPARE(cursor.activeIndex(), -1);
        QCOMPARE(cursor.nextIndex(), -1);
    }

    void unsyncedHasNoActiveLine()
    {
        LyricsDocument d = doc();
        d.synced = false;
        LyricsModel model;
        model.setDocument(d);
        LyricsCursor cursor;
        cursor.setModel(&model);
        cursor.setPositionMs(2000);
        QCOMPARE(cursor.activeIndex(), -1);
    }
};

QTEST_GUILESS_MAIN(TestCursor)
#include "tst_cursor.moc"
