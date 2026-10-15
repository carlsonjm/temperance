/* SPDX-License-Identifier: GPL-2.0-or-later */
// A Performance preset never saves a fan curve that leaves the fans off
// while the machine is hot, nor one the helper would refuse.
#include "fancurve.h"

#include <cstdlib>
#include <iostream>

namespace {
void require(bool value, const char *message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); }
}
} // namespace

int main()
{
    const QString starting = QStringLiteral("35:0,45:25,50:50,60:80,70:120,80:170,90:220,100:255");
    require(fanCurveAllowed(starting), "The starting curve was refused");
    require(fanCurveAllowed(QStringLiteral("30:0,40:0,50:0,60:0,70:51,80:51,90:51,100:51")),
        "A quiet curve that still turns the fans when hot was refused");
    require(!fanCurveAllowed(QStringLiteral("30:0,40:0,50:0,60:0,70:0,80:0,90:0,100:0")),
        "A curve with the fans off at every temperature was accepted");
    require(!fanCurveAllowed(QStringLiteral("30:0,40:0,50:0,60:0,70:50,80:60,90:70,100:80")),
        "A curve below a fifth of full speed at 70 degrees was accepted");
    require(!fanCurveAllowed(QStringLiteral("35:0,45:25,50:50,60:80,70:120,80:110,90:220,100:255")),
        "A curve whose speed falls was accepted");
    require(!fanCurveAllowed(QStringLiteral("35:0,45:25,45:50,60:80,70:120,80:170,90:220,100:255")),
        "A curve whose temperature does not rise was accepted");
    require(!fanCurveAllowed(QStringLiteral("35:0,45:25,50:50,60:80,70:120,80:170,90:220")),
        "A curve of seven points was accepted");
    require(!fanCurveAllowed(QStringLiteral("35:0,45:25,50:50,60:80,70:120,80:170,90:220,130:255")),
        "A point past 120 degrees was accepted");
    require(!fanCurveAllowed(QStringLiteral("35:0,45:25,50:50,60:80,70:120,80:170,90:220,100:256")),
        "A speed past full was accepted");
    require(!fanCurveAllowed(QStringLiteral("35:0,45:25,50:x,60:80,70:120,80:170,90:220,100:255")),
        "A point that is not a number was accepted");
    return EXIT_SUCCESS;
}
