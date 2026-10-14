/* SPDX-License-Identifier: GPL-2.0-or-later */
// The Status Bar runs only on the Plasma and Qt it was built for: an update
// ahead of the rebuild leaves it saying so rather than loading into a changed
// Plasma or Qt.
#include "buildmatch.h"

#include <cstdlib>
#include <iostream>

namespace {
void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); }
}
const QString built = QStringLiteral("6.7.4");
const QString qt = QStringLiteral("6.11.2");
} // namespace

int main()
{
    require(builtForRunning(built, built, qt, qt), "The Plasma and Qt it was built for were refused");
    require(!builtForRunning(built, QStringLiteral("6.8.0"), qt, qt), "A newer Plasma was accepted before the rebuild");
    require(!builtForRunning(built, QStringLiteral("6.7.5"), qt, qt), "A newer Plasma bugfix release was accepted");
    require(!builtForRunning(built, QStringLiteral("6.7.3"), qt, qt), "An older Plasma was accepted");
    require(!builtForRunning(built, built, qt, QStringLiteral("6.11.3")), "A newer Qt bugfix release was accepted");
    require(!builtForRunning(built, built, qt, QStringLiteral("6.10.2")), "An older Qt was accepted");
    require(builtForRunning(built, QString(), qt, qt), "A Plasma that did not say its version turned the Status Bar off");
    require(builtForRunning(built, built, qt, QStringLiteral(" ")), "A Qt that did not say its version turned the Status Bar off");
    require(builtForRunning(QString(), QStringLiteral("6.8.0"), QString(), QStringLiteral("6.12.0")),
        "A build that did not record its versions turned the Status Bar off");
    require(builtForRunning(built, QStringLiteral("6.7.4\n"), qt, qt), "A version with a trailing newline was refused");
    return EXIT_SUCCESS;
}
