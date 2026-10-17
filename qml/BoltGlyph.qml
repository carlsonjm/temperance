/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
import QtQuick

// The charging bolt, which stands in for a theme's status icon. It is drawn
// on an 18 px grid and scaled to its box.
Canvas {
    id: glyph

    property color ink: "#F8F8FF"

    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onInkChanged: requestPaint()
    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        ctx.scale(width / 18, height / 18);
        ctx.fillStyle = glyph.ink;
        ctx.beginPath();
        ctx.moveTo(10.5, 1); ctx.lineTo(3.5, 10);
        ctx.lineTo(8, 10); ctx.lineTo(7, 17);
        ctx.lineTo(14.5, 7); ctx.lineTo(10, 7);
        ctx.closePath(); ctx.fill();
    }
}
