// SPDX-License-Identifier: GPL-2.0-or-later
// The host's clock extent: asked once for an output, again only when the host
// says that output moved or the applet's output changes, and believed only
// for the schema and version this reads. A stand-in host answers on the
// test's private session bus.
#include "clockextent.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusMessage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

class StandInBar : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "co.goodinput.ContextBar")

public:
    QStringList asked;
    int right = 612;
    int version = 1;
    bool presenting = true;
    QString schema = QStringLiteral("co.goodinput.shuffle.clock-extent");
    bool fail = false;

    static QString reply(const QString &output, int right, int version = 1, bool presenting = true,
                         const QString &schema = QStringLiteral("co.goodinput.shuffle.clock-extent"))
    {
        const QJsonObject extent{
            {QStringLiteral("schema"), schema},
            {QStringLiteral("version"), version},
            {QStringLiteral("output"), output},
            {QStringLiteral("presenting"), presenting},
            {QStringLiteral("bar"), QJsonObject{{QStringLiteral("height"), 24}}},
            {QStringLiteral("clock"), QJsonObject{{QStringLiteral("left"), 1240}, {QStringLiteral("right"), 1292},
                                                   {QStringLiteral("center"), 1266}}},
            {QStringLiteral("available"), QJsonObject{{QStringLiteral("left"), 1224}, {QStringLiteral("right"), right}}},
        };
        return QString::fromUtf8(QJsonDocument(extent).toJson(QJsonDocument::Compact));
    }

public Q_SLOTS:
    QString clockExtent(const QString &outputName)
    {
        asked << outputName;
        if (fail) {
            sendErrorReply(QDBusError::Failed, QStringLiteral("no bar"));
            return {};
        }
        return reply(outputName, right, version, presenting, schema);
    }

Q_SIGNALS:
    void clockExtentChanged(const QString &outputName);
};

class ClockExtentTest : public QObject
{
    Q_OBJECT

    // The host lives on a connection of its own, so its replies and signals
    // cross the bus as a real host's do.
    QDBusConnection hostBus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("stand-in-bar"));
    StandInBar bar;

    bool publish()
    {
        return hostBus.registerObject(QStringLiteral("/ContextBar"), &bar, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals)
            && hostBus.registerService(QStringLiteral("co.goodinput.ContextBar"));
    }
    void withdraw()
    {
        hostBus.unregisterService(QStringLiteral("co.goodinput.ContextBar"));
        hostBus.unregisterObject(QStringLiteral("/ContextBar"));
    }
    // Long enough for a stray question to have crossed the bus and back.
    static void settle()
    {
        QTest::qWait(200);
    }

private Q_SLOTS:
    void readsOnlyWhatItKnows()
    {
        QCOMPARE(ClockExtent::parseAvailableWidth(StandInBar::reply(QStringLiteral("eDP-1"), 612)), 612);
        QCOMPARE(ClockExtent::parseAvailableWidth(StandInBar::reply(QStringLiteral("eDP-1"), 612, 2)), 0);
        QCOMPARE(ClockExtent::parseAvailableWidth(StandInBar::reply(QStringLiteral("eDP-1"), 612, 1, false)), 0);
        QCOMPARE(ClockExtent::parseAvailableWidth(StandInBar::reply(QStringLiteral("eDP-1"), 612, 1, true, QStringLiteral("other"))), 0);
        QCOMPARE(ClockExtent::parseAvailableWidth(StandInBar::reply(QStringLiteral("eDP-1"), 0)), 0);
        QCOMPARE(ClockExtent::parseAvailableWidth(QStringLiteral("not json")), 0);
        QCOMPARE(ClockExtent::parseAvailableWidth(QString()), 0);
    }

    void followsTheHost()
    {
        QVERIFY(hostBus.isConnected());
        ClockExtent extent;
        QSignalSpy changed(&extent, &ClockExtent::availableWidthChanged);

        // No host: the width stays the panel's to decide.
        extent.setOutput(QStringLiteral("eDP-1"));
        settle();
        QCOMPARE(extent.availableWidth(), 0);
        QCOMPARE(changed.count(), 0);

        // A host that comes up is asked once, for this output.
        QVERIFY(publish());
        QTRY_COMPARE(extent.availableWidth(), 612);
        QCOMPARE(bar.asked, QStringList{QStringLiteral("eDP-1")});

        // Nothing more is asked while nothing changes.
        settle();
        QCOMPARE(bar.asked.size(), 1);
        extent.setOutput(QStringLiteral("eDP-1"));
        settle();
        QCOMPARE(bar.asked.size(), 1);

        // Another output's change is not this one's.
        bar.right = 500;
        Q_EMIT bar.clockExtentChanged(QStringLiteral("HDMI-A-1"));
        settle();
        QCOMPARE(bar.asked.size(), 1);
        QCOMPARE(extent.availableWidth(), 612);

        // This output's change, or an unnamed one, is asked about.
        Q_EMIT bar.clockExtentChanged(QStringLiteral("eDP-1"));
        QTRY_COMPARE(extent.availableWidth(), 500);
        QCOMPARE(bar.asked.size(), 2);
        bar.right = 480;
        Q_EMIT bar.clockExtentChanged(QString());
        QTRY_COMPARE(extent.availableWidth(), 480);
        QCOMPARE(bar.asked.size(), 3);

        // Moving to another output asks for that one.
        bar.right = 700;
        extent.setOutput(QStringLiteral("HDMI-A-1"));
        QTRY_COMPARE(extent.availableWidth(), 700);
        QCOMPARE(bar.asked.last(), QStringLiteral("HDMI-A-1"));
        QCOMPARE(bar.asked.size(), 4);

        // A version this does not read, a bar not presenting, or an error,
        // hands the width back to the panel.
        bar.version = 2;
        Q_EMIT bar.clockExtentChanged(QStringLiteral("HDMI-A-1"));
        QTRY_COMPARE(extent.availableWidth(), 0);
        bar.version = 1;
        Q_EMIT bar.clockExtentChanged(QStringLiteral("HDMI-A-1"));
        QTRY_COMPARE(extent.availableWidth(), 700);
        bar.presenting = false;
        Q_EMIT bar.clockExtentChanged(QStringLiteral("HDMI-A-1"));
        QTRY_COMPARE(extent.availableWidth(), 0);
        bar.presenting = true;
        bar.fail = true;
        Q_EMIT bar.clockExtentChanged(QStringLiteral("HDMI-A-1"));
        settle();
        QCOMPARE(extent.availableWidth(), 0);
        bar.fail = false;
        Q_EMIT bar.clockExtentChanged(QStringLiteral("HDMI-A-1"));
        QTRY_COMPARE(extent.availableWidth(), 700);

        // A host that goes away leaves the width to the panel again.
        const int asked = bar.asked.size();
        withdraw();
        QTRY_COMPARE(extent.availableWidth(), 0);
        settle();
        QCOMPARE(bar.asked.size(), asked);
    }

    void cleanupTestCase()
    {
        withdraw();
        QDBusConnection::disconnectFromBus(QStringLiteral("stand-in-bar"));
    }
};

QTEST_GUILESS_MAIN(ClockExtentTest)
#include "ClockExtentTest.moc"
