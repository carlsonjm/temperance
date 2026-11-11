/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "keepawake.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusMessage>
#include <QSignalSpy>
#include <QTest>

// Stand-ins for Plasma's power management and screen locker, each on a
// connection of its own so every call crosses the test's session bus.
class FakeHold : public QObject, protected QDBusContext
{
    Q_OBJECT

public:
    QList<uint> held;
    QList<uint> released;
    QStringList reasons;
    bool refuse = false;

protected:
    uint inhibit(const QString &application, const QString &reason)
    {
        Q_UNUSED(application)
        if (refuse) {
            sendErrorReply(QDBusError::AccessDenied, QStringLiteral("refused"));
            return 0;
        }
        reasons.append(reason);
        held.append(++m_next);
        return m_next;
    }
    void uninhibit(uint cookie)
    {
        released.append(cookie);
        held.removeAll(cookie);
    }

private:
    uint m_next = 40;
};

class FakeSleep : public FakeHold
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.PowerManagement.Inhibit")

public Q_SLOTS:
    Q_SCRIPTABLE uint Inhibit(const QString &application, const QString &reason) { return inhibit(application, reason); }
    Q_SCRIPTABLE void UnInhibit(uint cookie) { uninhibit(cookie); }
};

class FakeScreen : public FakeHold
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.ScreenSaver")

public Q_SLOTS:
    Q_SCRIPTABLE uint Inhibit(const QString &application, const QString &reason) { return inhibit(application, reason); }
    Q_SCRIPTABLE void UnInhibit(uint cookie) { uninhibit(cookie); }
};

class Power
{
public:
    explicit Power(const QString &name)
        : bus(QDBusConnection::connectToBus(QDBusConnection::SessionBus, name))
        , m_name(name)
    {
    }
    ~Power()
    {
        down();
        QDBusConnection::disconnectFromBus(m_name);
    }
    bool up()
    {
        return bus.registerObject(QLatin1String(KeepAwake::SleepPath), &sleep, QDBusConnection::ExportScriptableSlots)
            && bus.registerObject(QLatin1String(KeepAwake::ScreenPath), &screen, QDBusConnection::ExportScriptableSlots)
            && bus.registerService(QLatin1String(KeepAwake::ScreenService))
            && bus.registerService(QLatin1String(KeepAwake::SleepService));
    }
    void down()
    {
        bus.unregisterService(QLatin1String(KeepAwake::SleepService));
        bus.unregisterService(QLatin1String(KeepAwake::ScreenService));
        bus.unregisterObject(QLatin1String(KeepAwake::SleepPath));
        bus.unregisterObject(QLatin1String(KeepAwake::ScreenPath));
    }

    QDBusConnection bus;
    FakeSleep sleep;
    FakeScreen screen;

private:
    QString m_name;
};

class KeepAwakeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void withoutPowerManagementItStaysOffAndHidden()
    {
        KeepAwake awake;
        awake.start();
        QVERIFY(!awake.available());
        awake.toggle();
        QVERIFY(!awake.active());
    }

    void oneTapOnOneTapOff()
    {
        Power power(QStringLiteral("power-on-off"));
        KeepAwake awake;
        QSignalSpy available(&awake, &KeepAwake::availableChanged);
        awake.start();
        QVERIFY(power.up());
        QTRY_VERIFY(awake.available());

        // The switch answers the tap at once; the holds follow.
        awake.toggle();
        QVERIFY(awake.active());
        QTRY_COMPARE(power.sleep.held.size(), 1);
        QTRY_COMPARE(power.screen.held.size(), 1);
        QVERIFY(!power.sleep.reasons.first().isEmpty());

        const uint sleepCookie = power.sleep.held.first();
        const uint screenCookie = power.screen.held.first();
        awake.toggle();
        QVERIFY(!awake.active());
        QTRY_COMPARE(power.sleep.released, QList<uint>{sleepCookie});
        QTRY_COMPARE(power.screen.released, QList<uint>{screenCookie});
        QVERIFY(power.sleep.held.isEmpty());
        QVERIFY(power.screen.held.isEmpty());
    }

    void quickTapsSettleOnTheLast()
    {
        Power power(QStringLiteral("power-quick"));
        QVERIFY(power.up());
        KeepAwake awake;
        awake.start();
        QVERIFY(awake.available());

        awake.toggle();
        awake.toggle();
        awake.toggle();
        QVERIFY(awake.active());
        QTest::qWait(300);
        QCOMPARE(power.sleep.held.size(), 1);
        QCOMPARE(power.screen.held.size(), 1);

        awake.toggle();
        awake.toggle();
        awake.toggle();
        QVERIFY(!awake.active());
        QTest::qWait(300);
        QVERIFY(power.sleep.held.isEmpty());
        QVERIFY(power.screen.held.isEmpty());
    }

    void aRefusalTurnsItBackOff()
    {
        Power power(QStringLiteral("power-refuse"));
        QVERIFY(power.up());
        power.sleep.refuse = true;
        KeepAwake awake;
        awake.start();
        QSignalSpy failed(&awake, &KeepAwake::failed);

        awake.toggle();
        QVERIFY(awake.active());
        QTRY_COMPARE(failed.count(), 1);
        QVERIFY(!awake.active());
        QTest::qWait(100);
        QVERIFY(power.screen.held.isEmpty());

        // A later tap asks again.
        power.sleep.refuse = false;
        awake.toggle();
        QTRY_COMPARE(power.sleep.held.size(), 1);
        QTRY_COMPARE(power.screen.held.size(), 1);
        QVERIFY(awake.active());
    }

    void powerManagementGoingTurnsItOff()
    {
        Power power(QStringLiteral("power-gone"));
        QVERIFY(power.up());
        KeepAwake awake;
        awake.start();
        awake.toggle();
        QTRY_COMPARE(power.screen.held.size(), 1);

        power.bus.unregisterService(QLatin1String(KeepAwake::SleepService));
        QTRY_VERIFY(!awake.available());
        QVERIFY(!awake.active());
        // The screen locker's hold is let go with it.
        QTRY_VERIFY(power.screen.held.isEmpty());
    }

    void removingTheWidgetLetsGo()
    {
        Power power(QStringLiteral("power-removed"));
        QVERIFY(power.up());
        {
            KeepAwake awake;
            awake.start();
            awake.toggle();
            QTRY_COMPARE(power.screen.held.size(), 1);
        }
        QTRY_VERIFY(power.sleep.held.isEmpty());
        QTRY_VERIFY(power.screen.held.isEmpty());
    }
};

QTEST_GUILESS_MAIN(KeepAwakeTest)
#include "KeepAwakeTest.moc"
