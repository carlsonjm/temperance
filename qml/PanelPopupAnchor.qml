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

    // How far the popup's visual parent stands off the bar, set by the bar
    // from Plasmoid.popupGap(). The default puts a page 10 px from a bar the
    // windows stop at, as Cards' default gutter puts a window.
    property real panelGap: 11

    width: vertical ? surfaceWidth : 1
    height: surfaceHeight
    x: vertical
        ? (panelLocation === PlasmaCore.Types.LeftEdge ? panelGap
            : panelLocation === PlasmaCore.Types.RightEdge ? -panelGap : 0)
        : surfaceWidth - width
    y: panelLocation === PlasmaCore.Types.TopEdge ? panelGap
        : panelLocation === PlasmaCore.Types.BottomEdge ? -panelGap : 0
}
