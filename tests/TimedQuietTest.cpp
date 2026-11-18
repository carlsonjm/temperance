/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "timedquiet.h"

#include <QTest>

static QDateTime at(int hour, int minute = 0) { return QDateTime(QDate(2026, 11, 18), QTime(hour, minute)); }

// A stand-in for the notification server and Plasma's setting: what the
// follower sets, announced back to it as both do.
struct Desk {
    TimedQuiet quiet;
    bool inhibited = false;
    QDateTime until;
    QDateTime now = at(10);
    int wake = -1;
    int cleared = 0;
    Desk()
    {
        QObject::connect(&quiet, &TimedQuiet::setInhibited, &quiet, [this](bool on) { set(on); });
        QObject::connect(&quiet, &TimedQuiet::wake, &quiet, [this](int milliseconds) { wake = milliseconds; });
        QObject::connect(&quiet, &TimedQuiet::clearUntil, &quiet, [this] {
            ++cleared;
            setUntil({});
        });
        quiet.inhibitedChanged(false, until, now);
        quiet.untilChanged({}, now);
    }
    void set(bool on)
    {
        if (on == inhibited) return;
        inhibited = on;
        quiet.inhibitedChanged(on, until, now);
    }
    void setUntil(const QDateTime &time)
    {
        until = time;
        quiet.untilChanged(time, now);
    }
    void pass(const QDateTime &time)
    {
        now = time;
        quiet.untilChanged(until, now);
    }
};

class TimedQuietTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void aTimeAheadIsQuietUntilIt()
    {
        Desk desk;
        QVERIFY(!desk.inhibited);
        QCOMPARE(desk.wake, -1);
        desk.setUntil(at(15));
        QVERIFY(desk.inhibited);
        QCOMPARE(desk.wake, 5 * 60 * 60 * 1000 + 1);
        desk.pass(at(15, 1));
        QVERIFY(!desk.inhibited);
        QCOMPARE(desk.wake, -1);
        // It ended by itself: the setting is left as Plasma leaves it.
        QCOMPARE(desk.cleared, 0);
    }
    void aTimeTakenOutEndsTheQuiet()
    {
        Desk desk;
        desk.setUntil(at(15));
        desk.setUntil({});
        QVERIFY(!desk.inhibited);
        QCOMPARE(desk.cleared, 0);
    }
    void turnedOffEndsTheTime()
    {
        Desk desk;
        desk.setUntil(at(15));
        desk.set(false);
        QCOMPARE(desk.cleared, 1);
        QVERIFY(!desk.until.isValid());
        QVERIFY(!desk.inhibited);
        QCOMPARE(desk.wake, -1);
    }
    void quietWithoutATimeIsLeftAlone()
    {
        Desk desk;
        desk.set(true);
        // Another setting changed; no time was ever set.
        desk.setUntil({});
        QVERIFY(desk.inhibited);
        desk.set(false);
        QCOMPARE(desk.cleared, 0);
    }
    void turnedOffEndsATimeItWasNotToldOf()
    {
        // Status's own switch writes the time and sets the quiet at once.
        Desk desk;
        desk.until = at(15);
        desk.set(true);
        desk.set(false);
        QCOMPARE(desk.cleared, 1);
        QVERIFY(!desk.until.isValid());
        QVERIFY(!desk.inhibited);
    }
    void aTimeGoneAtTheStartIsNotQuiet()
    {
        Desk desk;
        desk.setUntil(at(9));
        QVERIFY(!desk.inhibited);
        QCOMPARE(desk.wake, -1);
    }
    void aFarTimeIsLookedAtAgain()
    {
        Desk desk;
        desk.setUntil(QDateTime(QDate(2027, 11, 18), QTime(10, 0)));
        QVERIFY(desk.inhibited);
        QCOMPARE(desk.wake, 24 * 60 * 60 * 1000);
        desk.pass(QDateTime(QDate(2026, 11, 19), QTime(10, 0)));
        QVERIFY(desk.inhibited);
    }
    void aLaterTimeMovesTheEnd()
    {
        Desk desk;
        desk.setUntil(at(15));
        desk.setUntil(at(16));
        QVERIFY(desk.inhibited);
        QCOMPARE(desk.wake, 6 * 60 * 60 * 1000 + 1);
        QCOMPARE(desk.cleared, 0);
    }
};

QTEST_APPLESS_MAIN(TimedQuietTest)
#include "TimedQuietTest.moc"
