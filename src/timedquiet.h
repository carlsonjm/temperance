/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDateTime>
#include <QObject>

// Follows Plasma's own Do Not Disturb setting, the time it lasts until, as
// Plasma's notifications do. Plasma keeps only that time; whatever shows
// notifications turns it into the server's quiet. With this, Do Not Disturb
// set anywhere Plasma's is set, System Settings or another program, is
// Status's too, ends at its time, and survives signing out.
//
// Turning Do Not Disturb off any other way ends the time as well, so the
// setting and the bell never disagree.
class TimedQuiet : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    // The setting as it stands at `now`, at the start and at each change, and
    // again when the time asked for by wake() has passed.
    void untilChanged(const QDateTime &until, const QDateTime &now)
    {
        const bool ahead = until.isValid() && until > now;
        if (ahead) {
            m_timed = true;
            if (!m_inhibited) Q_EMIT setInhibited(true);
            // A timer counts in an int of milliseconds: a far end is looked
            // at again on the way.
            Q_EMIT wake(int(qMin<qint64>(now.msecsTo(until) + 1, 24 * 60 * 60 * 1000)));
            return;
        }
        Q_EMIT wake(-1);
        if (!m_timed) return;
        m_timed = false;
        if (m_inhibited) Q_EMIT setInhibited(false);
    }

    // The server's quiet, at the start and as it changes, with the setting
    // as it stands then.
    void inhibitedChanged(bool inhibited, const QDateTime &until, const QDateTime &now)
    {
        m_inhibited = inhibited;
        if (inhibited) return;
        // Turned off while a time was still ahead, whoever set it: the time
        // ends with it.
        m_timed = false;
        if (!until.isValid() || until <= now) return;
        Q_EMIT wake(-1);
        Q_EMIT clearUntil();
    }

Q_SIGNALS:
    // Set the server's quiet to this.
    void setInhibited(bool inhibited);
    // Take the time out of Plasma's setting.
    void clearUntil();
    // Look again after this many milliseconds; below zero, not at all.
    void wake(int milliseconds);

private:
    bool m_timed = false;
    bool m_inhibited = false;
};
