/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sharingquiet.h"

#include <QTest>

// A stand-in for the notification server's quiet: what the keeper sets, and
// the change it announces, as the server does.
struct Server {
    SharingQuiet keeper;
    bool inhibited = false;
    int cleared = 0;
    Server()
    {
        QObject::connect(&keeper, &SharingQuiet::setInhibited, &keeper, [this](bool on) { set(on); });
        QObject::connect(&keeper, &SharingQuiet::clearApplications, &keeper, [this] { ++cleared; });
    }
    void set(bool on)
    {
        if (on == inhibited) return;
        inhibited = on;
        keeper.inhibitedChanged(on);
    }
};

class SharingQuietTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theShareKeepsItQuietAndSaysSo()
    {
        Server server;
        server.keeper.start(false, false, false);
        QVERIFY(!server.inhibited);
        server.keeper.applicationsChanged(true, true);
        QVERIFY(server.inhibited);
        QVERIFY(server.keeper.wasSharing());
        server.keeper.applicationsChanged(false, false);
        QVERIFY(!server.inhibited);
        // Said after it ends, until the next quiet starts.
        QVERIFY(server.keeper.wasSharing());
        server.set(true);
        QVERIFY(!server.keeper.wasSharing());
        server.set(false);
        QVERIFY(!server.inhibited);
        QVERIFY(!server.keeper.wasSharing());
    }

    void doNotDisturbTheShareMeetsStaysOn()
    {
        Server server;
        server.keeper.start(false, false, false);
        server.set(true);
        server.keeper.applicationsChanged(true, true);
        server.keeper.applicationsChanged(false, false);
        // The person's own Do Not Disturb outlasts the share, and is theirs.
        QVERIFY(server.inhibited);
        QVERIFY(!server.keeper.wasSharing());
        server.set(false);
        QVERIFY(!server.inhibited);
        QVERIFY(!server.keeper.wasSharing());
        QCOMPARE(server.cleared, 0);
    }

    void turningItOffDuringTheShareLiftsIt()
    {
        Server server;
        server.keeper.start(false, false, false);
        server.keeper.applicationsChanged(true, true);
        QVERIFY(server.inhibited);
        server.set(false);
        QCOMPARE(server.cleared, 1);
        QVERIFY(!server.inhibited);
        QVERIFY(server.keeper.wasSharing());
        // The server then says nobody asks; it stays off.
        server.keeper.applicationsChanged(false, false);
        QVERIFY(!server.inhibited);
    }

    void anotherApplicationIsNoShare()
    {
        Server server;
        server.keeper.start(false, false, false);
        server.keeper.applicationsChanged(true, false);
        QVERIFY(server.inhibited);
        server.keeper.applicationsChanged(false, false);
        QVERIFY(!server.inhibited);
        QVERIFY(!server.keeper.wasSharing());
    }

    void alreadyQuietAtStart()
    {
        Server server;
        server.inhibited = true;
        server.keeper.start(true, false, false);
        QVERIFY(server.inhibited);
        server.keeper.applicationsChanged(true, true);
        server.keeper.applicationsChanged(false, false);
        QVERIFY(server.inhibited);
        Server sharing;
        sharing.keeper.start(false, true, true);
        QVERIFY(sharing.inhibited);
        QVERIFY(sharing.keeper.wasSharing());
    }

    void thePortalsReasonIsTheShare()
    {
        QVERIFY(SharingQuiet::sharingReason({QStringLiteral("Screen sharing in progress")}, QString()));
        QVERIFY(SharingQuiet::sharingReason({QStringLiteral("Presenting"), QStringLiteral("Partage d'écran en cours")},
                                            QStringLiteral("Partage d'écran en cours")));
        QVERIFY(!SharingQuiet::sharingReason({QStringLiteral("Presenting")}, QString()));
        QVERIFY(!SharingQuiet::sharingReason({}, QString()));
    }
};

QTEST_GUILESS_MAIN(SharingQuietTest)
#include "SharingQuietTest.moc"
