// SPDX-License-Identifier: AGPL-3.0-or-later
#include "lyrics/LrcParser.h"
#include "lyrics/TextDecoding.h"

#include <QFile>
#include <QtTest>

class TestTextDecoding : public QObject
{
    Q_OBJECT

private slots:
    void files_data()
    {
        QTest::addColumn<QString>("file");
        QTest::addColumn<QString>("encoding");
        QTest::addColumn<QString>("expected");
        QTest::newRow("gbk") << "gbk.lrc" << "GB18030" << QStringLiteral("月亮代表我的心");
        QTest::newRow("big5") << "big5.lrc" << "Big5" << QStringLiteral("月亮代表我的心");
        QTest::newRow("shift-jis") << "sjis.lrc" << "Shift_JIS" << QStringLiteral("さくらの花が咲いている");
        QTest::newRow("utf16 bom") << "utf16.lrc" << "UTF-16LE" << QStringLiteral("月亮代表我的心");
        QTest::newRow("utf8 bom") << "utf8bom.lrc" << "UTF-8" << QStringLiteral("月亮代表我的心");
    }

    void files()
    {
        QFETCH(QString, file);
        QFETCH(QString, encoding);
        QFETCH(QString, expected);
        QFile f(QStringLiteral(TEST_DATA_DIR "/") + file);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QString used;
        const QString text = TextDecoding::decode(f.readAll(), &used);
        QVERIFY2(text.contains(expected), qPrintable(text));
        QCOMPARE(used, encoding);
        QVERIFY(!text.contains(QLatin1Char('\r')));
        QVERIFY(!text.startsWith(QChar(0xfeff)));

        const auto doc = LrcParser::parse(text);
        QVERIFY(doc.synced);
        QCOMPARE(doc.lines.last().text, expected);
    }

    void plainUtf8()
    {
        QString used;
        QCOMPARE(TextDecoding::decode(QByteArray("[00:01.00]h\xc3\xa9llo"), &used), QStringLiteral("[00:01.00]héllo"));
        QCOMPARE(used, u"UTF-8");
    }
};

QTEST_GUILESS_MAIN(TestTextDecoding)
#include "tst_textdecoding.moc"
