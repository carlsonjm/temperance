/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QString>

class SessionManagement;

// The Control Center header's session controls. Lock, Sleep and Switch User
// act at once; Log Out, Restart and Shut Down ask through Plasma's own prompt
// (sessionaction.h).
class SessionActions : public QObject
{
    Q_OBJECT

public:
    explicit SessionActions(QObject *parent = nullptr);

    // Connects to Plasma's session management; until then every action fails.
    void start();

    Q_INVOKABLE void request(const QString &action);
    // Whether the machine can sleep now, as the session manager reports it.
    Q_INVOKABLE bool canSleep() const;

Q_SIGNALS:
    void failed();

private:
    SessionManagement *m_session = nullptr;
};
