/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sessionactions.h"

#include "debug.h"
#include "sessionaction.h"

#include <sessionmanagement.h>

#include <QDBusConnection>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

using namespace Qt::StringLiterals;

SessionActions::SessionActions(QObject *parent)
    : QObject(parent)
{
}

void SessionActions::start()
{
    if (!m_session) {
        m_session = new SessionManagement(this);
    }
}

void SessionActions::request(const QString &action)
{
    if (action == u"switchUser"_s) {
        if (m_session && m_session->canSwitchUser()) m_session->switchUser();
        else Q_EMIT failed();
        return;
    }
    // Sleeping asks nothing first either: everything open is there on waking.
    if (action == u"sleep"_s) {
        if (m_session && m_session->canSuspend()) m_session->suspend();
        else Q_EMIT failed();
        return;
    }
    // Locking asks nothing first: it loses no work.
    if (action == u"lock"_s) {
        if (m_session && m_session->canLock()) m_session->lock();
        else Q_EMIT failed();
        return;
    }
    const auto message = sessionActionPrompt(action);
    if (message.type() != QDBusMessage::MethodCallMessage) return;
    auto *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(message, 5000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher]() {
        const QDBusPendingReply<> reply = *watcher;
        if (reply.isError()) {
            qCWarning(SYSTEM_TRAY) << "Unable to show session confirmation:" << reply.error();
            Q_EMIT failed();
        }
        watcher->deleteLater();
    });
}

bool SessionActions::canSleep() const
{
    return m_session && m_session->canSuspend();
}
