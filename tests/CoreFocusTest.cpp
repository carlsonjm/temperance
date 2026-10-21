/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "corefocus.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QSignalSpy>
#include <QTest>

// A stand-in for Split Rock's Core on the test's own session bus.
class FakeCore : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.carlsonjm.SplitRock.Core")

public:
    QList<uint> ended;
    QList<uint> endedInView;
    int seen = 0;

public Q_SLOTS:
    Q_SCRIPTABLE void FocusEnded(uint notifications) { ended.append(notifications); }
    Q_SCRIPTABLE void FocusEndedInView(uint notifications) { endedInView.append(notifications); }
    Q_SCRIPTABLE void NotificationsSeen() { ++seen; }
};

class CoreFocusTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void withoutCoreNothingIsSentAndNothingBreaks()
    {
        CoreFocus focus;
        focus.start();
        focus.ended(3);
        focus.endedInView(3);
        focus.seen();
        QTest::qWait(100);
        QVERIFY(!QDBusConnection::sessionBus().interface()->isServiceRegistered(QLatin1String(CoreFocus::Service)));
    }

    void coreHearsTheCountAndAsksToOpen()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        FakeCore core;
        QVERIFY(bus.registerObject(QLatin1String(CoreFocus::Path), &core, QDBusConnection::ExportScriptableSlots));
        QVERIFY(bus.registerService(QLatin1String(CoreFocus::Service)));

        CoreFocus focus;
        focus.start();
        focus.ended(7);
        QTRY_COMPARE(core.ended, QList<uint>{7u});
        // A count below zero is sent as nothing came in.
        focus.ended(-2);
        QTRY_COMPARE(core.ended, (QList<uint>{7u, 0u}));
        // Ended with the history open, and the history opened.
        focus.endedInView(4);
        QTRY_COMPARE(core.endedInView, QList<uint>{4u});
        focus.seen();
        QTRY_COMPARE(core.seen, 1);
        QCOMPARE(core.ended.size(), 2);

        QSignalSpy asked(&focus, &CoreFocus::openRequested);
        const QDBusMessage signal = QDBusMessage::createSignal(QLatin1String(CoreFocus::Path), QLatin1String(CoreFocus::Interface),
                                                               QStringLiteral("OpenNotifications"));
        QVERIFY(bus.send(signal));
        QTRY_COMPARE(asked.count(), 1);

        bus.unregisterService(QLatin1String(CoreFocus::Service));
        bus.unregisterObject(QLatin1String(CoreFocus::Path));
    }
};

QTEST_GUILESS_MAIN(CoreFocusTest)
#include "CoreFocusTest.moc"
