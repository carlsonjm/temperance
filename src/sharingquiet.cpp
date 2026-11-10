/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sharingquiet.h"

using namespace Qt::StringLiterals;

SharingQuiet::SharingQuiet(QObject *parent)
    : QObject(parent)
{
}

void SharingQuiet::start(bool inhibited, bool applications, bool sharing)
{
    m_started = true;
    m_chosen = inhibited;
    m_applications = applications;
    m_sharing = applications && sharing;
    m_quiet = inhibited;
    m_spanSharing = false;
    m_spanChosen = inhibited;
    apply();
}

void SharingQuiet::inhibitedChanged(bool inhibited)
{
    if (!m_started || m_applying || inhibited == m_quiet) return;
    m_chosen = inhibited;
    if (!inhibited && m_applications) {
        // Do Not Disturb turned off while an application kept it on: the
        // person wants notifications back now, share or not.
        m_applications = false;
        m_sharing = false;
        Q_EMIT clearApplications();
    }
    settle(inhibited);
}

void SharingQuiet::applicationsChanged(bool applications, bool sharing)
{
    if (!m_started) return;
    m_applications = applications;
    m_sharing = applications && sharing;
    apply();
}

void SharingQuiet::apply()
{
    const bool want = m_chosen || m_applications;
    settle(want);
    m_applying = true;
    Q_EMIT setInhibited(want);
    m_applying = false;
}

// A quiet that starts forgets the last one; while quiet, it notes what keeps
// it so. What it noted stays after it ends, for whoever says it ended.
void SharingQuiet::settle(bool quiet)
{
    if (quiet && !m_quiet) {
        m_spanSharing = false;
        m_spanChosen = false;
    }
    m_quiet = quiet;
    if (!quiet) return;
    if (m_chosen) m_spanChosen = true;
    if (m_sharing) m_spanSharing = true;
}

bool SharingQuiet::sharingReason(const QStringList &reasons, const QString &localWords)
{
    // xdg-desktop-portal-kde's reason, in English and in the person's language.
    static const QString english = u"Screen sharing in progress"_s;
    for (const QString &reason : reasons) {
        if (reason == english || (!localWords.isEmpty() && reason == localWords)) return true;
    }
    return false;
}
