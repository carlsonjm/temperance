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
    // A drawing Temperance carries (StatusGlyphs.js), or suite-ethernet.
    required property string statusIcon
    required property string accessibleName
    property bool active: false
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
        // Drawn by Temperance, so no icon theme changes it. A symbolic
        // drawing fills its box, so it is drawn smaller than a theme's padded
        // 24 px status icon to make the same mark.
        SuiteIcon {
            visible: !statusIconButton.ethernet
            anchors.centerIn: parent
            glyph: statusIconButton.ethernet ? "nm-signal-100-symbolic" : statusIconButton.statusIcon
            width: bar.symbolicIconSize
            height: width
        }
    }
    HoverHandler { id: statusIconButtonHover }
    TapHandler { id: statusIconButtonTap; onTapped: statusIconButton.activate() }
    PlasmaComponents.ToolTip { text: statusIconButton.accessibleName }
}
