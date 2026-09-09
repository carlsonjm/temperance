/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: ISC
*/
pragma ComponentBehavior: Bound

import QtQuick

Canvas {
    id: glyph

    property color glyphColor: "#F8F8FF"
    property bool slashed: false
    property real clapperProgress: 1
    property real strokeWidth: 1.65

    onGlyphColorChanged: requestPaint()
    onSlashedChanged: requestPaint()
    onClapperProgressChanged: requestPaint()
    onStrokeWidthChanged: requestPaint()

    Behavior on clapperProgress {
        NumberAnimation { duration: 160; easing.type: Easing.InOutCubic }
    }

    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        ctx.strokeStyle = glyph.glyphColor;
        ctx.lineWidth = glyph.strokeWidth;
        ctx.lineCap = "round";
        ctx.lineJoin = "round";

        // Lucide Bell geometry on a normalized 24 px grid.
        ctx.beginPath();
        ctx.moveTo(width * 0.136, height * 0.639);
        ctx.bezierCurveTo(width * 0.191, height * 0.582, width * 0.25, height * 0.521,
            width * 0.25, height * 0.333);
        ctx.bezierCurveTo(width * 0.25, height * 0.195, width * 0.362, height * 0.083,
            width * 0.5, height * 0.083);
        ctx.bezierCurveTo(width * 0.638, height * 0.083, width * 0.75, height * 0.195,
            width * 0.75, height * 0.333);
        ctx.bezierCurveTo(width * 0.75, height * 0.521, width * 0.809, height * 0.582,
            width * 0.864, height * 0.639);
        ctx.bezierCurveTo(width * 0.889, height * 0.665, width * 0.871, height * 0.708,
            width * 0.833, height * 0.708);
        ctx.lineTo(width * 0.167, height * 0.708);
        ctx.bezierCurveTo(width * 0.129, height * 0.708, width * 0.111, height * 0.665,
            width * 0.136, height * 0.639);
        ctx.stroke();

        if (glyph.clapperProgress > 0.01) {
            ctx.save();
            ctx.globalAlpha = glyph.clapperProgress;
            ctx.beginPath();
            ctx.moveTo(width * 0.428, height * 0.875);
            ctx.bezierCurveTo(width * 0.461, height * 0.932, width * 0.539, height * 0.932,
                width * 0.572, height * 0.875);
            ctx.stroke();
            ctx.restore();
        }

        if (glyph.slashed) {
            ctx.beginPath();
            ctx.moveTo(width * 0.16, height * 0.14);
            ctx.lineTo(width * 0.84, height * 0.86);
            ctx.stroke();
        }
    }
}
