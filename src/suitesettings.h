/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KIO/CommandLauncherJob>
#include <KLocalizedString>
#include <KService>
#include <KShell>

#include <Plasma/Applet>

#include <QAction>
#include <QMetaObject>
#include <QStandardPaths>

// Where Shuffle Settings is installed, the widget's Configure opens it at
// the widget's own page; where it isn't, Configure opens the widget's own
// settings as always. It is looked for at each press, so installing or
// removing it needs no restart, and nothing else changes with it.
//
// Plasma connects Configure to the widget's own settings by the slot's name.
// That one connection is taken over; a Plasma that connects it some other way
// keeps its own settings page, which is the safe way round.
inline void openConfigureInSuiteSettings(Plasma::Applet *applet, const QString &page)
{
    QAction *configure = applet->internalAction(QStringLiteral("configure"));
    if (!configure || !QObject::disconnect(configure, SIGNAL(triggered()), applet, SLOT(requestConfiguration()))) {
        return;
    }
    QObject::connect(configure, &QAction::triggered, applet, [applet, page] {
        const KService::Ptr settings = KService::serviceByDesktopName(QStringLiteral("studio.warbler.Shuffle.Settings"));
        const QString program = settings ? KShell::splitArgs(settings->exec()).value(0) : QString();
        if (program.isEmpty()) {
            QMetaObject::invokeMethod(applet, "requestConfiguration");
            return;
        }
        auto *job = new KIO::CommandLauncherJob(program, {page});
        job->setDesktopName(settings->desktopEntryName());
        job->start();
    });
}

// Inside Shuffle the widget goes by Shuffle's name for it, in its title and
// its Configure entry; elsewhere it keeps its own. Shuffle is installed where
// its Bottom Surface is, the one part every Shuffle install has.
inline void useShuffleName(Plasma::Applet *applet, const QString &name)
{
    if (QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                               QStringLiteral("plasma/plasmoids/studio.warbler.shuffle.bottomsurface"),
                               QStandardPaths::LocateDirectory)
            .isEmpty()) {
        return;
    }
    applet->setTitle(name);
    if (QAction *configure = applet->internalAction(QStringLiteral("configure"))) {
        configure->setText(i18nc("@action:inmenu %1 is the widget's name", "Configure %1…", name));
    }
}
