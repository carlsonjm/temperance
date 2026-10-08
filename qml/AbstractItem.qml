/*
    SPDX-FileCopyrightText: 2016 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2020 Konrad Materka <materka@gmail.com>
    SPDX-FileCopyrightText: 2020 Nate Graham <nate@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid

PlasmaCore.ToolTipArea {
    id: abstractItem

    StatusColors {
        id: tone
        theme: abstractItem.Kirigami.Theme
    }

    required property int index
    required property var model
    required property int status
    required property int effectiveStatus

    required property string itemId
    /*required*/ property alias text: label.text

    // subclasses need to bind these tooltip properties
    required mainText
    required subText
    required textFormat

    readonly property alias iconContainer: iconContainer
    readonly property bool inHiddenLayout: effectiveStatus === PlasmaCore.Types.PassiveStatus
    readonly property bool inVisibleLayout: effectiveStatus === PlasmaCore.Types.ActiveStatus

    property bool effectivePressed: false
    // A tile's tooltip waits for the pointer to move over it. A popup shown
    // again is handed the point of the last tap or click as a hover, with no
    // pointer there, and that alone must not raise a tooltip.
    property bool pointerMoved: false
    property var pointerOrigin: null

    function notePointer(x, y) {
        const point = mouseArea.mapToItem(null, x, y);
        if (!pointerOrigin) {
            pointerOrigin = point;
            return;
        }
        if (pointerMoved || mouseArea.pressed
                || Math.hypot(point.x - pointerOrigin.x, point.y - pointerOrigin.y) <= Kirigami.Units.smallSpacing)
            return;
        pointerMoved = true;
        tooltipDelay.restart();
    }

    function forgetPointer() {
        pointerMoved = false;
        pointerOrigin = null;
        tooltipDelay.stop();
    }

    Timer {
        id: tooltipDelay
        interval: Kirigami.Units.toolTipDelay
        onTriggered: if (mouseArea.containsMouse && !mouseArea.pressed && abstractItem.active) {
            abstractItem.showToolTip()
        }
    }

    Connections {
        target: abstractItem.Window.window
        function onVisibleChanged() { abstractItem.forgetPointer() }
    }
    property bool cardBackground: false
    property string presentationIcon: ""
    property string presentationText: ""
    property bool inlinePresentation: false
    property var inlineIcon: presentationIcon

    function suiteSentenceCaseLabel(value) {
        if (value === "Disks & Devices") return i18n("Disks & devices");
        if (value === "Display Configuration") return i18n("Display configuration");
        return value;
    }

    // Keep these in sync with HiddenItems.qml
    readonly property int margins: Kirigami.Units.smallSpacing
    readonly property int maxTextLines: cardBackground ? 1 : 2

    // input agnostic way to trigger the main action
    signal activated(var pos)

    // proxy signals for MouseArea
    signal clicked(var mouse)
    signal pressed(var mouse)
    signal wheel(var wheel)
    signal contextMenu(var mouse)

    PulseAnimation {
        targetItem: iconContainer
        // Only pulse where it can be seen: an item in a closed popup or a
        // hidden window would otherwise animate unseen for as long as it asks.
        running: (abstractItem.status === PlasmaCore.Types.NeedsAttentionStatus
                || abstractItem.status === PlasmaCore.Types.RequiresAttentionStatus)
            && Kirigami.Units.longDuration > 0
            && abstractItem.visible
            && !!abstractItem.Window.window && abstractItem.Window.window.visible
    }

    MouseArea {
        id: mouseArea
        propagateComposedEvents: true
        // This needs to be above applets when it's in the grid hidden area
        // so that it can receive hover events while the mouse is over an applet,
        // but below them on regular systray, so collapsing works
        z: abstractItem.inHiddenLayout ? 1 : 0
        anchors.fill: abstractItem
        hoverEnabled: true
        drag.filterChildren: true
        // Necessary to make the whole delegate area forward all mouse events
        acceptedButtons: Qt.AllButtons
        // Using onPositionChanged instead of onEntered because changing the
        // index in a scrollable view also changes the view position.
        // onEntered will change the index while the items are scrolling,
        // making it harder to scroll.
        onPositionChanged: mouse => {
            abstractItem.notePointer(mouse.x, mouse.y)
            if (abstractItem.inHiddenLayout) {
                root.hiddenLayout.currentIndex = abstractItem.index
            }
        }
        onEntered: abstractItem.notePointer(mouseX, mouseY)
        onExited: abstractItem.forgetPointer()
        onClicked: mouse => { abstractItem.clicked(mouse) }
        onPressed: mouse => {
            if (abstractItem.inHiddenLayout) {
                root.hiddenLayout.currentIndex = abstractItem.index
            }
            abstractItem.hideImmediately()
            abstractItem.forgetPointer()
            abstractItem.pressed(mouse)
        }
        onPressAndHold: mouse => {
            if (mouse.button === Qt.LeftButton) {
                abstractItem.contextMenu(mouse)
            }
        }
        onWheel: wheel => {
            abstractItem.wheel(wheel);
            //Don't accept the event in order to make the scrolling by mouse wheel working
            //for the parent scrollview this icon is in.
            wheel.accepted = false;
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: abstractItem.cardBackground ? 0 : Kirigami.Units.smallSpacing / 2
        radius: abstractItem.cardBackground ? height / 2 : Kirigami.Units.cornerRadius
        visible: abstractItem.inHiddenLayout
        color: abstractItem.effectivePressed || mouseArea.containsMouse
            ? tone.wash(0.12)
            : (abstractItem.cardBackground ? tone.wash(0.07) : "transparent")
    }

    ColumnLayout {
        visible: !abstractItem.inlinePresentation
        anchors.fill: abstractItem
        anchors.margins: abstractItem.inHiddenLayout
            ? (abstractItem.cardBackground ? Math.round(abstractItem.margins / 2) : abstractItem.margins) : 0

        spacing: 1

        FocusScope {
            id: iconContainer
            scale: (abstractItem.effectivePressed || mouseArea.containsPress) ? 0.8 : 1

            activeFocusOnTab: !abstractItem.inHiddenLayout
            focus: true // Required in HiddenItemsView so keyboard events can be forwarded to this item
            Accessible.name: abstractItem.text
            Accessible.description: abstractItem.subText
            Accessible.role: Accessible.Button
            Accessible.onPressAction: abstractItem.activated(Plasmoid.popupPosition(iconContainer, iconContainer.width/2, iconContainer.height/2));

            Behavior on scale {
                ScaleAnimator {
                    duration: Kirigami.Units.longDuration
                    easing.type: (abstractItem.effectivePressed || mouseArea.containsPress) ? Easing.OutCubic : Easing.InCubic
                }
            }

            Keys.onPressed: event => {
                switch (event.key) {
                    case Qt.Key_Space:
                    case Qt.Key_Enter:
                    case Qt.Key_Return:
                    case Qt.Key_Select:
                        abstractItem.activated(Qt.point(width/2, height/2));
                        break;
                    case Qt.Key_Menu:
                        abstractItem.contextMenu(null);
                        event.accepted = true;
                        break;
                }
            }

            property alias container: abstractItem
            property alias inVisibleLayout: abstractItem.inVisibleLayout
            readonly property int size: abstractItem.inVisibleLayout ? root.itemSize : Kirigami.Units.iconSizes.medium

            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            implicitWidth: root.vertical && abstractItem.inVisibleLayout ? abstractItem.width : size
            implicitHeight: !root.vertical && abstractItem.inVisibleLayout ? abstractItem.height : size
        }
        PlasmaComponents3.Label {
            id: label

            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            maximumLineCount: abstractItem.maxTextLines

            visible: abstractItem.inHiddenLayout

            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            textFormat: Text.PlainText
            wrapMode: Text.Wrap

            opacity: visible ? 1 : 0
            Behavior on opacity {
                NumberAnimation {
                    duration: Kirigami.Units.longDuration
                    easing.type: Easing.InOutQuad
                }
            }
        }
    }

    RowLayout {
        visible: abstractItem.inlinePresentation
        anchors.centerIn: parent
        width: Math.min(implicitWidth, abstractItem.width - Kirigami.Units.largeSpacing * 2)
        spacing: 8

        Kirigami.Icon {
            source: abstractItem.inlineIcon
            implicitWidth: Kirigami.Units.iconSizes.smallMedium
            implicitHeight: implicitWidth
        }
        PlasmaComponents3.Label {
            Layout.fillWidth: true
            text: abstractItem.presentationText
                || abstractItem.suiteSentenceCaseLabel(abstractItem.text)
            maximumLineCount: 1
            elide: Text.ElideRight
        }
    }
}
