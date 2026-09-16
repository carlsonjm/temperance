/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
import QtQuick
import org.kde.plasma.core as PlasmaCore

Item {
    required property bool vertical
    required property real surfaceWidth
    required property real surfaceHeight
    required property int panelLocation

    readonly property real panelGap: 18

    width: vertical ? surfaceWidth : 1
    height: surfaceHeight
    x: vertical
        ? (panelLocation === PlasmaCore.Types.LeftEdge ? panelGap
            : panelLocation === PlasmaCore.Types.RightEdge ? -panelGap : 0)
        : surfaceWidth - width
    y: panelLocation === PlasmaCore.Types.TopEdge ? panelGap
        : panelLocation === PlasmaCore.Types.BottomEdge ? -panelGap : 0
}
