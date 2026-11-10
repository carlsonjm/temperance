/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "corefocus.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

using namespace Qt::StringLiterals;

CoreFocus::CoreFocus(QObject *parent)
    : QObject(parent)
{
}

void CoreFocus::start()
{
    if (m_started) return;
    m_started = QDBusConnection::sessionBus().connect(QLatin1String(Service), QLatin1String(Path),
                                                      QLatin1String(Interface), u"OpenNotifications"_s,
                                                      this, SLOT(requested()));
}

void CoreFocus::ended(int notifications)
{
    send(u"FocusEnded"_s, {QVariant::fromValue(uint(qMax(0, notifications)))});
}

void CoreFocus::endedInView(int notifications)
{
    send(u"FocusEndedInView"_s, {QVariant::fromValue(uint(qMax(0, notifications)))});
}

void CoreFocus::sharingEnded(int notifications)
{
    send(u"SharingEnded"_s, {QVariant::fromValue(uint(qMax(0, notifications)))}, u"FocusEnded"_s);
}

void CoreFocus::sharingEndedInView(int notifications)
{
    send(u"SharingEndedInView"_s, {QVariant::fromValue(uint(qMax(0, notifications)))}, u"FocusEndedInView"_s);
}

void CoreFocus::seen()
{
    send(u"NotificationsSeen"_s);
}

void CoreFocus::send(const QString &method, const QVariantList &arguments, const QString &otherwise)
{
    auto call = QDBusMessage::createMethodCall(QLatin1String(Service), QLatin1String(Path),
                                               QLatin1String(Interface), method);
    call.setArguments(arguments);
    call.setAutoStartService(false);
    if (otherwise.isEmpty()) {
        QDBusConnection::sessionBus().send(call);
        return;
    }
    // A Core without the method says so; it hears the other one instead.
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, arguments, otherwise] {
        watcher->deleteLater();
        const QDBusPendingReply<> reply = *watcher;
        if (reply.isError() && reply.error().type() == QDBusError::UnknownMethod)
            send(otherwise, arguments);
    });
}

void CoreFocus::requested()
{
    Q_EMIT openRequested();
}
