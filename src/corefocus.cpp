/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "corefocus.h"

#include <QDBusConnection>
#include <QDBusMessage>

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
    auto call = QDBusMessage::createMethodCall(QLatin1String(Service), QLatin1String(Path),
                                               QLatin1String(Interface), u"FocusEnded"_s);
    call.setArguments({QVariant::fromValue(uint(qMax(0, notifications)))});
    call.setAutoStartService(false);
    QDBusConnection::sessionBus().send(call);
}

void CoreFocus::requested()
{
    Q_EMIT openRequested();
}
