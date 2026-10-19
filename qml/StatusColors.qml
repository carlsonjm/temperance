/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import org.kde.kirigami as Kirigami

// Temperance's colours, read from the theme of the item they paint. On a dark
// ground they are the suite's values, leaning toward the ground's colour as
// far as the ground leans from grey, so a plain dark style looks as it always
// has and a tinted one carries its tint; on a light ground the ink and the
// ground come from the theme and every wash, control and line is that ink
// laid over the ground.
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
    readonly property color surface: dark ? suite("#141414") : ground
    readonly property color control: dark ? suite("#242424") : over(0.07)
    readonly property color controlHover: dark ? suite("#333333") : over(0.12)
    readonly property color controlPressed: dark ? suite("#4A4A4A") : over(0.22)
    readonly property color line: dark ? suite("#333333") : over(0.16)
    readonly property color divider: dark ? suite("#5a5a5a") : over(0.32)

    // How far the ground leans from grey: nothing on a plain style, the
    // look's tint on a tinted one, so the suite's dark greys lean with it.
    readonly property var lean: {
        const mean = (ground.r + ground.g + ground.b) / 3;
        return [ground.r - mean, ground.g - mean, ground.b - mean];
    }

    // One of the suite's dark values, leaning as the ground does.
    function suite(value) {
        const c = Qt.color(value);
        const channel = (v, i) => Math.round(Math.min(1, Math.max(0, v + lean[i])) * 255) / 255;
        return Qt.rgba(channel(c.r, 0), channel(c.g, 1), channel(c.b, 2), c.a);
    }

    // The ink at a given strength, for see-through washes and fills.
    function wash(alpha) {
        return Qt.rgba(ink.r, ink.g, ink.b, alpha);
    }

    // The ink at a given strength laid solid over the ground.
    function over(alpha) {
        return Qt.tint(ground, wash(alpha));
    }
}
