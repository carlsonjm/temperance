/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
pragma ComponentBehavior: Bound

import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

// The slim row's status controls drawn finger-sized, in a strip that grows
// out of the bar while a finger pulls down from it. The bar is a few
// millimetres tall, so the finger chooses here instead: the control under it
// lifts, its neighbours a little less, and its name shows beneath it.
//
// The strip only draws. The finger stays with the panel that took its press,
// and the panel says how far the strip has grown and where the finger is.
Item {
    id: strip
    objectName: "temperance-magnifier"

    // The panel's root, for what each control shows now.
    required property Item status
    // One entry for each control in the bar, left to right: its `key` (the
    // page it opens), its `name`, the bar's `control`, and where that control
    // sits, `barX` and `barY`, in the strip's coordinates.
    property var entries: []
    // How far the strip has grown out of the bar, 0 to 1.
    property real growth: 0
    // The finger's x in the strip's coordinates, and the entry under it.
    property real fingerX: -1
    property int selected: -1
    // Along the bottom of the screen the strip grows up out of the bar.
    property bool upward: false

    // A finger is about 16 mm wide, 78 logical pixels on the tablet the row
    // is drawn for, so each control is that far from the next. Its mark is
    // about half that, as in the bar, leaving room to lift without touching
    // its neighbour.
    readonly property int pitch: 78
    readonly property int iconSize: 40
    // The control under the finger is drawn this much larger; its neighbours
    // a share of it that falls away over one and a half controls.
    readonly property real lift: 0.3
    readonly property real reach: 1.5
    readonly property int padding: 8
    readonly property int iconZone: 60
    readonly property int labelHeight: 18
    // The size the bar draws its marks at, which the strip grows from.
    readonly property int barIconSize: 16

    readonly property real eased: 1 - Math.pow(1 - Math.max(0, Math.min(1, growth)), 2)

    readonly property var columnWidths: entries.map(entry => entry.key === "calendar"
        ? Math.max(pitch, Math.ceil(clockMetrics.advanceWidth) + 24) : pitch)
    readonly property var restCentres: {
        const centres = [];
        let left = padding;
        for (const columnWidth of columnWidths) {
            centres.push(left + columnWidth / 2);
            left += columnWidth;
        }
        return centres;
    }
    readonly property real iconCentreY: upward ? height - padding - iconZone / 2 : padding + iconZone / 2

    width: 2 * padding + columnWidths.reduce((sum, columnWidth) => sum + columnWidth, 0)
    height: 2 * padding + iconZone + labelHeight

    // The entry whose column holds an x in the strip, the end columns taking
    // the strip's padding too; none off either end.
    function indexAt(x) {
        if (x < 0 || x >= width || entries.length === 0) return -1;
        let right = padding;
        for (let i = 0; i < columnWidths.length; ++i) {
            right += columnWidths[i];
            if (x < right || i === columnWidths.length - 1) return i;
        }
        return -1;
    }

    // How much of the lift an entry takes, by its distance from the finger,
    // as a dock magnifies the icons near the pointer.
    function liftAt(index) {
        if (selected < 0 || index >= restCentres.length) return 0;
        const distance = Math.abs(fingerX - restCentres[index]) / pitch;
        return distance >= reach ? 0 : (1 + Math.cos(Math.PI * distance / reach)) / 2;
    }

    function mix(from, to, amount) {
        return from + (to - from) * amount;
    }

    StatusColors {
        id: tone
        theme: strip.Kirigami.Theme
    }

    TextMetrics {
        id: clockMetrics
        font: Qt.font({ family: strip.clockFamily(), pixelSize: 22 })
        text: strip.clockText()
    }

    function clockEntry() {
        return entries.find(entry => entry.key === "calendar");
    }
    function clockText() {
        const entry = clockEntry();
        if (!entry) return "";
        return entry.control.showTime ? entry.control.timeText : entry.control.dateText;
    }
    function clockFamily() {
        const entry = clockEntry();
        return entry ? entry.control.family : Kirigami.Theme.defaultFont.family;
    }

    // The ground, grown from the bar's run of controls to the whole strip.
    Rectangle {
        id: ground
        readonly property real barLeft: strip.entries.length
            ? strip.entries[0].barX - strip.status.statusPitch / 2 : 0
        readonly property real barRight: strip.entries.length
            ? strip.entries[strip.entries.length - 1].barX + strip.status.statusPitch / 2 : strip.width
        readonly property real barHeight: strip.status.rowHeight
        readonly property real barTop: strip.entries.length
            ? strip.entries[0].barY - barHeight / 2 : 0
        x: strip.mix(barLeft, 0, strip.eased)
        width: strip.mix(barRight - barLeft, strip.width, strip.eased)
        height: strip.mix(barHeight, strip.height, strip.eased)
        y: strip.mix(barTop, strip.upward ? strip.height - height : 0, strip.eased)
        radius: strip.mix(barHeight / 2, 16, strip.eased)
        color: tone.surface
        border.width: 1
        border.color: tone.line
        opacity: Math.min(1, strip.eased * 3)
    }

    // Behind the control under the finger.
    Rectangle {
        objectName: "temperance-magnifier-selection"
        visible: strip.selected >= 0 && strip.selected < strip.restCentres.length
        readonly property real target: visible ? strip.restCentres[strip.selected] : 0
        width: visible ? strip.columnWidths[strip.selected] - 8 : 0
        height: strip.iconZone - 4
        x: target - width / 2
        y: strip.iconCentreY - height / 2
        radius: 14
        color: tone.control
        opacity: strip.eased
        Behavior on x {
            enabled: strip.status.motionEnabled
            NumberAnimation { duration: Kirigami.Units.shortDuration; easing.type: Easing.OutCubic }
        }
    }

    Repeater {
        model: strip.entries

        delegate: Item {
            id: face
            required property var modelData
            required property int index
            readonly property string key: modelData.key
            readonly property var control: modelData.control
            readonly property bool chosen: strip.selected === index
            // The bar draws text a little larger than its marks.
            readonly property real barScale: key === "calendar" ? 13 / 22
                : key === "battery" && !strip.status.batteryOnAC ? 12 / 22
                : strip.barIconSize / strip.iconSize
            property real liftScale: 1 + strip.lift * strip.liftAt(index)
            objectName: "temperance-magnifier-" + key
            width: strip.columnWidths[index] ?? strip.pitch
            height: strip.iconZone
            x: strip.mix(modelData.barX, strip.restCentres[index] ?? 0, strip.eased) - width / 2
            y: strip.mix(modelData.barY, strip.iconCentreY, strip.eased) - height / 2
            scale: strip.mix(barScale, 1, strip.eased) * strip.mix(1, liftScale, strip.eased)
            Behavior on liftScale {
                enabled: strip.status.motionEnabled
                NumberAnimation { duration: Kirigami.Units.shortDuration; easing.type: Easing.OutCubic }
            }

            // The bell, or with something unread its chip; slashed under
            // do not disturb.
            BellGlyph {
                visible: face.key === "notifications" && !strip.status.bellShowsCount
                anchors.centerIn: parent
                width: strip.iconSize
                height: strip.iconSize
                strokeWidth: 3
                glyphColor: tone.text
                clapperProgress: 1
                slashed: strip.status.doNotDisturb
            }
            Rectangle {
                visible: face.key === "notifications" && strip.status.bellShowsCount
                anchors.centerIn: parent
                height: 36
                width: Math.max(height, chipLabel.implicitWidth + 18)
                radius: height / 2
                color: Kirigami.Theme.highlightColor
                PlasmaComponents.Label {
                    id: chipLabel
                    anchors.centerIn: parent
                    text: strip.status.unreadText
                    textFormat: Text.PlainText
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                    font.features: { "tnum": 1 }
                    color: Kirigami.Theme.highlightedTextColor
                }
            }

            Kirigami.Icon {
                visible: face.key === "weather"
                    || (face.key === "control" && !face.control.ethernet)
                anchors.centerIn: parent
                width: strip.iconSize
                height: strip.iconSize
                roundToIconSize: false
                source: face.key === "weather" ? strip.status.weatherIcon()
                    : face.key === "control" ? face.control.statusIcon : ""
                isMask: face.key === "control" && face.control.symbolic
                color: tone.text
            }
            EthernetGlyph {
                visible: face.key === "control" && face.control.ethernet
                anchors.centerIn: parent
                width: strip.iconSize * 7 / 8
                height: width
                strokeWidth: 3
                glyphColor: tone.text
            }

            TrayGlyph {
                visible: face.key === "tray"
                anchors.centerIn: parent
                anchors.verticalCenterOffset: 2
                mark: Math.round(9 * strip.iconSize / strip.barIconSize)
                ink: tone.text
                ring: face.key === "tray" && face.control.active
            }

            BoltGlyph {
                visible: face.key === "battery" && strip.status.batteryOnAC
                anchors.centerIn: parent
                width: strip.iconSize
                height: strip.iconSize
                ink: tone.text
            }
            PlasmaComponents.Label {
                visible: face.key === "battery" && !strip.status.batteryOnAC
                anchors.centerIn: parent
                text: face.key === "battery" ? strip.status.batteryLabel() || "—" : ""
                textFormat: Text.PlainText
                font.pixelSize: 22
                color: tone.text
            }

            PlasmaComponents.Label {
                visible: face.key === "calendar"
                anchors.centerIn: parent
                text: face.key === "calendar" ? strip.clockText() : ""
                textFormat: Text.PlainText
                font: clockMetrics.font
                color: tone.text
            }
        }
    }

    // The name of the control under the finger, where the tooltip would be
    // for a pointer.
    PlasmaComponents.Label {
        id: nameLabel
        objectName: "temperance-magnifier-label"
        readonly property bool shown: strip.selected >= 0 && strip.selected < strip.entries.length
        text: shown ? strip.entries[strip.selected].name : ""
        textFormat: Text.PlainText
        font.pixelSize: 13
        font.weight: Font.Medium
        color: tone.text
        height: strip.labelHeight
        verticalAlignment: Text.AlignVCenter
        x: shown ? Math.max(strip.padding, Math.min(strip.width - strip.padding - implicitWidth,
            strip.restCentres[strip.selected] - implicitWidth / 2)) : 0
        y: strip.upward ? strip.padding : strip.padding + strip.iconZone
        opacity: shown ? Math.max(0, (strip.eased - 0.6) / 0.4) : 0
    }
}
