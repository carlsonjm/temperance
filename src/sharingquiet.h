/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QStringList>

// Keeps notifications quiet while an application asks for it, as Plasma's own
// notifications do. Plasma's portal asks while the screen is shared, when
// Plasma's "Do not disturb while sharing the screen" is on; a presentation
// can ask too. The notification server only lists those requests: whatever
// shows notifications turns them into the quiet itself.
//
// Quiet is the person's own Do Not Disturb or an application's request.
// Turning Do Not Disturb off while an application keeps it on lifts the
// application's request too, so the person always has a way out. When a quiet
// ends, wasSharing() says whether it was the screen share that kept it, with
// the person never having chosen it meanwhile.
class SharingQuiet : public QObject
{
    Q_OBJECT

public:
    explicit SharingQuiet(QObject *parent = nullptr);

    // Where things stand when it starts: the server's quiet, taken as the
    // person's own, and the applications asking.
    void start(bool inhibited, bool applications, bool sharing);

    // The server's quiet changed. A change this did not make is the person's
    // own Do Not Disturb, set or lifted.
    void inhibitedChanged(bool inhibited);
    // Applications asking for quiet: whether any does, and whether one of
    // them is the screen share.
    void applicationsChanged(bool applications, bool sharing);

    bool quiet() const { return m_quiet; }
    // The last quiet, or the one now, was the screen share's alone.
    bool wasSharing() const { return m_spanSharing && !m_spanChosen; }

    // Whether these reasons, as the server lists them, include Plasma's
    // portal sharing the screen. localWords is the portal's reason in the
    // person's language.
    static bool sharingReason(const QStringList &reasons, const QString &localWords);

Q_SIGNALS:
    // Set the server's quiet to this.
    void setInhibited(bool inhibited);
    // Lift every application's request.
    void clearApplications();

private:
    void apply();
    void settle(bool quiet);

    bool m_started = false;
    bool m_chosen = false;
    bool m_applications = false;
    bool m_sharing = false;
    bool m_quiet = false;
    bool m_applying = false;
    bool m_spanSharing = false;
    bool m_spanChosen = false;
};
