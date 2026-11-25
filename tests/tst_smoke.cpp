// SPDX-License-Identifier: AGPL-3.0-or-later
#include <QtTest>

class TestSmoke : public QObject
{
    Q_OBJECT
private slots:
    void builds() { QVERIFY(true); }
};

QTEST_GUILESS_MAIN(TestSmoke)
#include "tst_smoke.moc"
