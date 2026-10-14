/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QString>

// Temperance reaches into Qt's and Plasma's private parts, which change between
// releases, so it runs only on the Plasma and Qt it was built for. On another
// the Status Bar touches none of them and says it needs an update, which leaves
// notifications free for Plasma's own. A version either side does not know is
// taken as the one built for, so nothing turns off by accident.
inline bool versionMatches(const QString &built, const QString &running)
{
    const QString builtFor = built.trimmed();
    const QString runs = running.trimmed();
    return builtFor.isEmpty() || runs.isEmpty() || runs == builtFor;
}

inline bool builtForRunning(const QString &builtPlasma, const QString &runningPlasma,
                            const QString &builtQt, const QString &runningQt)
{
    return versionMatches(builtPlasma, runningPlasma) && versionMatches(builtQt, runningQt);
}
