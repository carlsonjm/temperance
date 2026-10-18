/*
    SPDX-FileCopyrightText: 2011 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2020 Konrad Materka <materka@gmail.com>
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

Item {
    id: statusIconButton
    // The Status Bar whose measures this follows.
    required property Item bar
    required property StatusColors tone
    required property var statusIcon
    required property string accessibleName
    property bool active: false
    // A single-colour symbolic drawing fills its box, so it is asked for
    // smaller than a theme's padded 24 px status icon to draw the same mark.
    readonly property bool symbolic: String(statusIcon).endsWith("-symbolic")
    // A wired connection draws Lucide's ethernet port, as the bell is drawn.
    readonly property bool ethernet: statusIcon === "suite-ethernet"
    signal triggered()
    function activate() { triggered(); }
    Layout.preferredWidth: bar.statusPitch
    Layout.minimumWidth: bar.statusPitch
    Layout.maximumWidth: bar.statusPitch
    Layout.fillHeight: true
    Accessible.name: accessibleName
    Accessible.role: Accessible.Button

    StatusFace {
        anchors.fill: parent
        touched: statusIconButtonTap.pressed || bar.reachedControl === statusIconButton
        hovered: statusIconButtonHover.hovered
        EthernetGlyph {
            visible: statusIconButton.ethernet
            anchors.centerIn: parent
            width: bar.wiredGlyphSize
            height: bar.wiredGlyphSize
            strokeWidth: bar.bellStroke
        }
        Kirigami.Icon {
            visible: !statusIconButton.ethernet
            anchors.centerIn: parent
            source: statusIconButton.statusIcon
            // 24 and 16 are sizes themes draw status icons at, so the
            // theme's own drawing is used rather than one scaled from
            // another size; Kirigami would otherwise round a size down.
            implicitWidth: statusIconButton.symbolic ? bar.symbolicIconSize : bar.statusIconSize
            implicitHeight: implicitWidth
            roundToIconSize: false
            // Some themes' symbolic drawings carry a fixed colour rather
            // than the text colour Kirigami replaces.
            isMask: statusIconButton.symbolic
            color: tone.text
        }
    }
    HoverHandler { id: statusIconButtonHover }
    TapHandler { id: statusIconButtonTap; onTapped: statusIconButton.activate() }
    PlasmaComponents.ToolTip { text: statusIconButton.accessibleName }
}
