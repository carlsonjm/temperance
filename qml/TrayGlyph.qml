/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
import QtQuick

// The tray's mark: a rounded triangle, ringed while the tray is open. The
// canvas leaves room around the mark for the ring.
Canvas {
    id: glyph

    // The mark's size.
    property real mark: 12
    property color ink: "#F8F8FF"
    // Ringed while the tray is open; the ring widens as the ripple runs.
    property bool ring: false
    property real rippleProgress: 0

    width: Math.round(mark * 28 / 9)
    height: width
    onWidthChanged: requestPaint()
    onInkChanged: requestPaint()
    onRingChanged: requestPaint()
    onRippleProgressChanged: requestPaint()
    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        function triangle(size) {
            const cx = width / 2;
            const cy = height / 2;
            const vertices = [[cx, cy - size * 0.55],
                [cx + size * 0.6, cy + size * 0.4],
                [cx - size * 0.6, cy + size * 0.4]];
            // Round the path itself, not just the stroke join.
            const inset = size * 0.16;
            ctx.beginPath();
            for (let i = 0; i < 3; ++i) {
                const v = vertices[i];
                const prev = vertices[(i + 2) % 3];
                const next = vertices[(i + 1) % 3];
                const a = inset / Math.hypot(prev[0] - v[0], prev[1] - v[1]);
                const b = inset / Math.hypot(next[0] - v[0], next[1] - v[1]);
                const x = v[0] + (prev[0] - v[0]) * a;
                const y = v[1] + (prev[1] - v[1]) * a;
                if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
                ctx.quadraticCurveTo(v[0], v[1],
                    v[0] + (next[0] - v[0]) * b,
                    v[1] + (next[1] - v[1]) * b);
            }
            ctx.closePath();
        }
        ctx.lineJoin = "round";
        ctx.fillStyle = glyph.ink;
        ctx.strokeStyle = glyph.ink;
        if (glyph.ring) {
            // Offset the SAME path with a stroke band, rather than
            // scaling vertices. This keeps clearance uniform around
            // both the straight edges and the rounded corners.
            const offset = (2 + glyph.rippleProgress * 1.6) * mark / 9;
            ctx.globalAlpha = 0.8 - glyph.rippleProgress * 0.3;
            ctx.lineWidth = offset * 2 + 1;
            triangle(mark);
            ctx.stroke();
            ctx.globalCompositeOperation = "destination-out";
            ctx.globalAlpha = 1;
            ctx.lineWidth = offset * 2 - 1;
            ctx.fill();
            ctx.stroke();
            ctx.globalCompositeOperation = "source-over";
        }
        ctx.globalAlpha = 1;
        ctx.lineWidth = 2 * mark / 9;
        triangle(mark); ctx.fill(); ctx.stroke();
    }
}
