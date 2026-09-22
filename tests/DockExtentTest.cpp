/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

// The published-extent path, against a stub surface. It covers what the
// contract promises a consumer: an unknown major version is refused rather
// than guessed at, a surface that is not presenting an output reserves
// nothing on it, and an unowned name means compose exactly as on a plain
// panel. None of it samples on a timer.

#include "dockextentreader.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QEventLoop>
#include <QObject>
#include <QSignalSpy>
#include <QTimer>

#include <cstdlib>

namespace
{
constexpr auto kService = "studio.warbler.BottomSurface";
constexpr auto kPath = "/BottomSurface";
}

class StubSurface : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "studio.warbler.BottomSurface")

public:
    QString payload;

public Q_SLOTS:
    Q_SCRIPTABLE QString dockExtent(const QString &outputName)
    {
        Q_UNUSED(outputName)
        return payload;
    }

Q_SIGNALS:
    Q_SCRIPTABLE void dockExtentChanged(const QString &outputName);
};

namespace
{
bool settle(int milliseconds = 300)
{
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
    return true;
}

QString payloadFor(const char *schema, int version, bool presenting,
                   int left, int right)
{
    return QStringLiteral("{\"schema\":\"%1\",\"version\":%2,"
                          "\"output\":\"TEST-0\",\"presenting\":%3,"
                          "\"band\":{\"height\":60},"
                          "\"dock\":{\"left\":%4,\"right\":%5,\"center\":0},"
                          "\"available\":{\"left\":0,\"right\":0}}")
        .arg(QString::fromLatin1(schema))
        .arg(version)
        .arg(presenting ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(left)
        .arg(right);
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        // No session bus in this environment is not a failure of the code.
        return EXIT_SUCCESS;
    }

    StubSurface stub;
    stub.payload = payloadFor("studio.warbler.shuffle.dock-extent", 1, true,
                              900, 1660);

    if (!bus.registerObject(QString::fromLatin1(kPath), &stub,
                            QDBusConnection::ExportScriptableSlots
                                | QDBusConnection::ExportScriptableSignals)) {
        return EXIT_FAILURE;
    }
    if (!bus.registerService(QString::fromLatin1(kService))) {
        return EXIT_FAILURE;
    }

    DockExtentReader reader;
    QSignalSpy changed(&reader, &DockExtentReader::extentChanged);

    // No screen is named TEST-0 here, so ask for it the way the change signal
    // does rather than relying on the startup sweep over real outputs.
    QMetaObject::invokeMethod(&reader, "refresh",
                              Q_ARG(QString, QStringLiteral("TEST-0")));
    settle();

    const auto right = reader.rightEdgeFor(QStringLiteral("TEST-0"));
    if (!right.has_value() || qRound(*right) != 1660) {
        return 2;
    }
    const auto left = reader.leftEdgeFor(QStringLiteral("TEST-0"));
    if (!left.has_value() || qRound(*left) != 900) {
        return 3;
    }

    // An unknown major version is refused, and refusing leaves what was known
    // untouched rather than replacing it with a guess.
    stub.payload = payloadFor("studio.warbler.shuffle.dock-extent", 2, true,
                              10, 20);
    QMetaObject::invokeMethod(&reader, "refresh",
                              Q_ARG(QString, QStringLiteral("TEST-0")));
    settle();
    if (qRound(*reader.rightEdgeFor(QStringLiteral("TEST-0"))) != 1660) {
        return 4;
    }

    // A surface that is not presenting this output reserves nothing on it.
    stub.payload = payloadFor("studio.warbler.shuffle.dock-extent", 1, false,
                              900, 1660);
    QMetaObject::invokeMethod(&reader, "refresh",
                              Q_ARG(QString, QStringLiteral("TEST-0")));
    settle();
    if (reader.rightEdgeFor(QStringLiteral("TEST-0")).has_value()) {
        return 5;
    }

    // And an unowned name reports nothing at all, which is what composing as
    // on a plain panel depends on.
    stub.payload = payloadFor("studio.warbler.shuffle.dock-extent", 1, true,
                              900, 1660);
    QMetaObject::invokeMethod(&reader, "refresh",
                              Q_ARG(QString, QStringLiteral("TEST-0")));
    settle();
    if (!reader.rightEdgeFor(QStringLiteral("TEST-0")).has_value()) {
        return 6;
    }
    bus.unregisterService(QString::fromLatin1(kService));
    settle();
    if (reader.rightEdgeFor(QStringLiteral("TEST-0")).has_value()) {
        return 7;
    }

    if (changed.isEmpty()) {
        return 8;
    }
    return EXIT_SUCCESS;
}

#include "DockExtentTest.moc"
