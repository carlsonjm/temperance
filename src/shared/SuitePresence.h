/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Shared by Tettegouche and Temperance. This folder in Tettegouche is where
// the shared code changes; Temperance carries a byte-for-byte copy, which its
// tools/shared.sh records and checks, so one change reaches both.

#pragma once

#include <QStandardPaths>
#include <QString>

namespace suite
{
// Shuffle is installed where its Bottom Surface is, the one part every
// Shuffle install has.
inline bool shuffleInstalled()
{
    return !QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                   QStringLiteral("plasma/plasmoids/co.goodinput.shuffle.bottomsurface"),
                                   QStandardPaths::LocateDirectory)
                .isEmpty();
}
}
