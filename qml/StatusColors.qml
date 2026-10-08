/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import org.kde.kirigami as Kirigami

// Temperance's colours, read from the theme of the item they paint. On a dark
// ground they are the suite's fixed values, so a dark Plasma style looks as it
// always has; on a light ground the ink and the ground come from the theme and
// every wash, control and line is that ink laid over the ground.
QtObject {
    id: tone
    objectName: "temperanceStatusColors"

    // The Kirigami.Theme of the item being coloured: `item.Kirigami.Theme`.
    required property QtObject theme

    readonly property color ground: theme ? theme.backgroundColor : "#141414"
    readonly property color themeInk: theme ? theme.textColor : "#F8F8FF"
    readonly property bool dark: Kirigami.ColorUtils.brightnessForColor(ground) === Kirigami.ColorUtils.Dark

    // White on dark, the theme's text colour on light: the base of every wash.
    readonly property color ink: dark ? "#FFFFFF" : themeInk
    readonly property color text: dark ? "#F8F8FF" : themeInk
    readonly property color secondaryText: dark ? "#A8FFFFFF" : wash(0.66)
    readonly property color faintText: dark ? "#88FFFFFF" : wash(0.53)
    readonly property color errorText: dark ? "#FFB4A9" : (theme ? theme.negativeTextColor : "#DA4453")

    // The popup's own panel, and the solid controls and lines drawn on it.
    readonly property color surface: dark ? "#141414" : ground
    readonly property color control: dark ? "#242424" : over(0.07)
    readonly property color controlHover: dark ? "#333333" : over(0.12)
    readonly property color controlPressed: dark ? "#4A4A4A" : over(0.22)
    readonly property color line: dark ? "#333333" : over(0.16)
    readonly property color divider: dark ? "#5a5a5a" : over(0.32)

    // The ink at a given strength, for see-through washes and fills.
    function wash(alpha) {
        return Qt.rgba(ink.r, ink.g, ink.b, alpha);
    }

    // The ink at a given strength laid solid over the ground.
    function over(alpha) {
        return Qt.tint(ground, wash(alpha));
    }
}
