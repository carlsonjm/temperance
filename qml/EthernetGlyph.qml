/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: ISC
*/
import QtQuick

// Lucide's ethernet port, drawn with the bell's line: its geometry is
// Lucide's 24 px grid, scaled to the box, and its stroke stays 2 px at any
// size. At the network icon's 21 px it is no wider or taller than the bell.
Canvas {
    id: glyph

    property color glyphColor: "#F8F8FF"
    property real strokeWidth: 2

    onGlyphColorChanged: requestPaint()
    onStrokeWidthChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        const scale = Math.min(width, height) / 24;
        ctx.translate((width - 24 * scale) / 2, (height - 24 * scale) / 2);
        ctx.scale(scale, scale);
        ctx.strokeStyle = glyph.glyphColor;
        ctx.lineWidth = glyph.strokeWidth / scale;
        ctx.lineCap = "round";
        ctx.lineJoin = "round";
        // Lucide's path, its arc flags written out: Qt's reader misreads
        // them run together.
        ctx.path = "M 19 17 a 2 2 0 0 0 -1.765 1.059 l -0.47 0.882 A 2 2 0 0 1 15 20"
            + " H 9 a 2 2 0 0 1 -1.765 -1.059 l -0.47 -0.882 A 2 2 0 0 0 5 17 H 4"
            + " a 2 2 0 0 1 -2 -2 V 6 a 2 2 0 0 1 2 -2 h 16 a 2 2 0 0 1 2 2 v 9"
            + " a 2 2 0 0 1 -2 2 z M 6 8 v 1 M 10 8 v 1 M 14 8 v 1 M 18 8 v 1";
        ctx.stroke();
    }
}
