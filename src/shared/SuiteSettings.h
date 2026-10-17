/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Shared by Tettegouche and Temperance. This folder in Tettegouche is where
// the shared code changes; Temperance carries a byte-for-byte copy, which its
// tools/shared.sh records and checks, so one change reaches both.

#pragma once

#include <KIO/CommandLauncherJob>
#include <KService>
#include <KShell>

#include <Plasma/Applet>

#include <QAction>
#include <QDebug>
#include <QMetaObject>

namespace suite
{
// Where Shuffle Settings is installed, the widget's Configure opens it at
// the widget's own page; where it isn't, Configure opens the widget's own
// settings as always. It is looked for at each press, so installing or
// removing it needs no restart, and nothing else changes with it.
//
// Plasma connects Configure to the widget's own settings by the slot's name.
// That one connection is taken over; a Plasma that connects it some other way
// keeps its own settings page, which is the safe way round, and the log says
// so. False when Configure was not taken over.
inline bool openConfigureInSuiteSettings(Plasma::Applet *applet, const QString &page)
{
    QAction *configure = applet->internalAction(QStringLiteral("configure"));
    if (!configure) {
        return false;
    }
    if (!QObject::disconnect(configure, SIGNAL(triggered()), applet, SLOT(requestConfiguration()))) {
        qWarning().noquote() << applet->pluginMetaData().pluginId()
                             << "could not take over Configure; it opens the widget's own settings";
        return false;
    }
    QObject::connect(configure, &QAction::triggered, applet, [applet, page] {
        const KService::Ptr settings = KService::serviceByDesktopName(QStringLiteral("co.goodinput.Shuffle.Settings"));
        const QString program = settings ? KShell::splitArgs(settings->exec()).value(0) : QString();
        if (program.isEmpty()) {
            QMetaObject::invokeMethod(applet, "requestConfiguration");
            return;
        }
        auto *job = new KIO::CommandLauncherJob(program, {page});
        job->setDesktopName(settings->desktopEntryName());
        job->start();
    });
    return true;
}
}
