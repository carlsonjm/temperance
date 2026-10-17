/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
import QtQuick
import org.kde.kirigami as Kirigami

// A speaker, with its waves, or crossed out while muted.
Canvas {
    id: speakerGlyph
    property bool muted: false
    implicitWidth: Kirigami.Units.iconSizes.smallMedium
    implicitHeight: implicitWidth
    property color ink: "#F8F8FF"
    onMutedChanged: requestPaint()
    onInkChanged: requestPaint()
    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        ctx.strokeStyle = ink;
        ctx.fillStyle = ink;
        ctx.lineWidth = 1.8;
        ctx.lineCap = "round";
        ctx.lineJoin = "round";

        ctx.beginPath();
        ctx.moveTo(width * 0.17, height * 0.42);
        ctx.lineTo(width * 0.34, height * 0.42);
        ctx.lineTo(width * 0.54, height * 0.25);
        ctx.lineTo(width * 0.54, height * 0.75);
        ctx.lineTo(width * 0.34, height * 0.58);
        ctx.lineTo(width * 0.17, height * 0.58);
        ctx.closePath();
        ctx.fill();

        if (muted) {
            ctx.beginPath();
            ctx.moveTo(width * 0.66, height * 0.39);
            ctx.lineTo(width * 0.86, height * 0.61);
            ctx.moveTo(width * 0.86, height * 0.39);
            ctx.lineTo(width * 0.66, height * 0.61);
            ctx.stroke();
        } else {
            ctx.beginPath();
            ctx.arc(width * 0.52, height * 0.5, width * 0.18, -0.82, 0.82);
            ctx.stroke();
            ctx.beginPath();
            ctx.arc(width * 0.52, height * 0.5, width * 0.32, -0.72, 0.72);
            ctx.stroke();
        }
    }
}
