/* SPDX-License-Identifier: GPL-2.0-or-later */
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kitemmodels as KItemModels
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import org.kde.plasma.private.brightnesscontrolplugin as Brightness
import org.kde.plasma.workspace.dbus as DBus

Item {
    id: page

    required property var activateAppletById
    required property var controlCenterModel
    required property var performancePresets
    required property string activePerformancePreset
    required property var applyPerformancePreset
    required property color accentColor
    readonly property color accentTextColor: (accentColor.r * 0.299 + accentColor.g * 0.587 + accentColor.b * 0.114) > 0.58
        ? "#102729" : "#FFFFFF"

    implicitWidth: Kirigami.Units.gridUnit * 24
    implicitHeight: contentLayout.implicitHeight + 24

    property string displayName: ""
    property int displayBrightness: 0
    property int displayBrightnessMax: 100
    readonly property bool hasBattery: Boolean(battery.properties.IsPresent)
    readonly property bool hasPerformanceProfiles: performancePresets.length > 0
    readonly property real batteryPercent: Number(battery.properties.Percentage || 0)
    readonly property int batteryState: Number(battery.properties.State || 0)
    readonly property real batterySeconds: batteryState === 1
        ? Number(battery.properties.TimeToFull || 0)
        : Number(battery.properties.TimeToEmpty || 0)

    function updateBrightness() {
        const displays = screenBrightness.displays;
        if (!displays || displays.rowCount() < 1) {
            displayName = "";
            displayBrightness = 0;
            displayBrightnessMax = 100;
            return;
        }
        const idx = displays.index(0, 0);
        displayName = displays.data(idx, displays.KItemModels.KRoleNames.role("displayName"));
        displayBrightness = displays.data(idx, displays.KItemModels.KRoleNames.role("brightness"));
        displayBrightnessMax = displays.data(idx, displays.KItemModels.KRoleNames.role("maxBrightness")) || 100;
    }

    function batteryStateText() {
        if (batteryState === 1) return i18n("Charging");
        if (batteryState === 2) return i18n("On battery");
        if (batteryState === 4) return i18n("Fully charged");
        return i18n("Battery");
    }

    function durationText(seconds) {
        if (!seconds || seconds < 60) return "";
        const hours = Math.floor(seconds / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        if (hours > 0) return i18n("%1h %2m remaining", hours, minutes);
        return i18n("%1m remaining", minutes);
    }

    component ActionTile: Rectangle {
        id: actionTile
        required property string tileIcon
        required property string tileText
        signal triggered()
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.preferredWidth: 0
        Layout.preferredHeight: Kirigami.Units.gridUnit * 2.75
        clip: true
        radius: height / 2
        color: actionHover.hovered ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, 0.07)
        RowLayout {
            anchors.centerIn: parent
            spacing: Kirigami.Units.smallSpacing
            Kirigami.Icon {
                source: actionTile.tileIcon
                implicitWidth: Kirigami.Units.iconSizes.smallMedium
                implicitHeight: implicitWidth
            }
            PlasmaComponents.Label {
                text: actionTile.tileText
                maximumLineCount: 1
                elide: Text.ElideRight
            }
        }
        HoverHandler { id: actionHover }
        TapHandler { onTapped: actionTile.triggered() }
    }

    component AppletTile: Item {
        required property var tileModel
        required property string tileIcon
        required property string tileText
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.preferredWidth: 0
        Layout.preferredHeight: Kirigami.Units.gridUnit * 2.75
        clip: true
        Repeater {
            id: tileRepeater
            anchors.fill: parent
            model: parent.tileModel
            delegate: ItemLoader {
                width: parent.width
                height: parent.height
                presentationStatus: PlasmaCore.Types.PassiveStatus
                cardBackground: true
                presentationIcon: tileRepeater.parent.tileIcon
                presentationText: tileRepeater.parent.tileText
                inlinePresentation: true
            }
        }
    }

    component BatteryGlyph: Item {
        property real level: 0
        implicitWidth: 23
        implicitHeight: 14
        Rectangle {
            x: 0
            y: 1
            width: 19
            height: 12
            radius: 3
            color: "transparent"
            border.width: 2
            border.color: "white"
            Rectangle {
                x: 3
                y: 3
                width: Math.max(2, 11 * Math.min(100, Math.max(0, parent.parent.level)) / 100)
                height: 6
                radius: 1.5
                color: "white"
            }
        }
        Rectangle { x: 20; y: 4; width: 3; height: 6; radius: 1; color: "white" }
    }

    component AccentSlider: PlasmaComponents.Slider {
        id: accentSlider
        background: Item {
            x: accentSlider.leftPadding
            y: accentSlider.topPadding + Math.round((accentSlider.availableHeight - height) / 2)
            width: accentSlider.availableWidth
            height: 4
            Rectangle {
                anchors.fill: parent
                radius: height / 2
                color: Qt.rgba(1, 1, 1, 0.07)
            }
            Rectangle {
                width: Math.max(height, accentSlider.visualPosition * parent.width)
                height: parent.height
                radius: height / 2
                color: page.accentColor
            }
        }
        handle: Rectangle {
            x: accentSlider.leftPadding + accentSlider.visualPosition * (accentSlider.availableWidth - width)
            y: accentSlider.topPadding + (accentSlider.availableHeight - height) / 2
            implicitWidth: 16
            implicitHeight: 16
            radius: width / 2
            color: accentSlider.pressed ? Qt.lighter(page.accentColor, 1.18) : page.accentColor
        }
    }

    component SpeakerGlyph: Canvas {
        id: speakerGlyph
        property bool muted: false
        implicitWidth: Kirigami.Units.iconSizes.smallMedium
        implicitHeight: implicitWidth
        onMutedChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            ctx.strokeStyle = Kirigami.Theme.textColor;
            ctx.fillStyle = Kirigami.Theme.textColor;
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

    DBus.Properties {
        id: battery
        busType: DBus.BusType.System
        service: "org.freedesktop.UPower"
        path: "/org/freedesktop/UPower/devices/DisplayDevice"
        iface: "org.freedesktop.UPower.Device"
    }

    Brightness.ScreenBrightnessControl { id: screenBrightness; isSilent: true }

    Connections {
        target: screenBrightness.displays
        function onDataChanged() { page.updateBrightness(); }
        function onModelReset() { page.updateBrightness(); }
        function onRowsInserted() { page.updateBrightness(); }
        function onRowsRemoved() { page.updateBrightness(); }
    }
    Connections {
        target: screenBrightness
        function onIsBrightnessAvailableChanged() { page.updateBrightness(); }
    }
    Component.onCompleted: Qt.callLater(updateBrightness)

    ColumnLayout {
        id: contentLayout
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Kirigami.Units.largeSpacing
        anchors.rightMargin: Kirigami.Units.largeSpacing
        anchors.topMargin: 12
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing
            PlasmaComponents.ToolButton {
                text: i18n("Open sound settings")
                display: PlasmaComponents.AbstractButton.IconOnly
                onClicked: page.activateAppletById("org.kde.plasma.volume")
                contentItem: SpeakerGlyph { muted: Plasmoid.volumeMuted }
                PlasmaComponents.ToolTip { text: parent.text }
            }
            AccentSlider {
                id: volumeSlider
                Layout.fillWidth: true
                from: 0
                to: 100
                stepSize: 1
                enabled: Plasmoid.volumeAvailable
                onMoved: Plasmoid.setVolumePercent(Math.round(value))
            }
            Binding { target: volumeSlider; property: "value"; value: Plasmoid.volumePercent; when: !volumeSlider.pressed }
            PlasmaComponents.Label {
                Layout.minimumWidth: Kirigami.Units.gridUnit * 2.4
                horizontalAlignment: Text.AlignRight
                text: Plasmoid.volumeAvailable ? Plasmoid.volumePercent + "%" : "—"
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 12
            spacing: Kirigami.Units.smallSpacing
            PlasmaComponents.ToolButton {
                icon.name: "brightness-high"
                text: i18n("Open brightness settings")
                display: PlasmaComponents.AbstractButton.IconOnly
                onClicked: page.activateAppletById("org.kde.plasma.brightness")
                PlasmaComponents.ToolTip { text: parent.text }
            }
            AccentSlider {
                id: brightnessSlider
                Layout.fillWidth: true
                from: 0
                to: page.displayBrightnessMax
                stepSize: Math.max(1, page.displayBrightnessMax / 100)
                enabled: screenBrightness.isBrightnessAvailable && page.displayName !== ""
                onMoved: screenBrightness.setBrightness(page.displayName, Math.round(value))
            }
            Binding { target: brightnessSlider; property: "value"; value: page.displayBrightness; when: !brightnessSlider.pressed }
            PlasmaComponents.Label {
                Layout.minimumWidth: Kirigami.Units.gridUnit * 2.4
                horizontalAlignment: Text.AlignRight
                text: page.displayBrightnessMax ? Math.round(page.displayBrightness / page.displayBrightnessMax * 100) + "%" : "—"
            }
        }

        GridLayout {
            id: controlGrid
            readonly property int visibleRows: Math.ceil(controlRepeater.count / columns)
            readonly property real tileHeight: Kirigami.Units.gridUnit * 2.75
            implicitHeight: visibleRows > 0
                ? visibleRows * tileHeight + (visibleRows - 1) * rowSpacing : 0
            Layout.fillWidth: true
            Layout.topMargin: visibleRows > 0 ? 24 : 0
            Layout.preferredHeight: implicitHeight
            columns: 2
            columnSpacing: 8
            rowSpacing: 8

            Repeater {
                id: controlRepeater
                model: page.controlCenterModel
                delegate: Item {
                    id: controlItem
                    required property int index
                    required property int effectiveStatus
                    required property var model
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: 0
                    Layout.preferredHeight: controlGrid.tileHeight
                    ItemLoader {
                        anchors.fill: parent
                        index: controlItem.index
                        effectiveStatus: controlItem.effectiveStatus
                        model: controlItem.model
                        presentationStatus: PlasmaCore.Types.PassiveStatus
                        cardBackground: true
                        inlinePresentation: true
                    }
                }
            }
        }

        RowLayout {
            visible: page.hasBattery || page.hasPerformanceProfiles
            Layout.fillWidth: true
            Layout.topMargin: visible ? 8 : 0
            spacing: 8

            Rectangle {
                visible: page.hasBattery
                Layout.fillWidth: true
                Layout.preferredHeight: Kirigami.Units.gridUnit * 2.75
                radius: height / 2
                color: Qt.rgba(1, 1, 1, 0.08)
                RowLayout {
                    anchors.centerIn: parent
                    spacing: 12
                    BatteryGlyph { level: page.batteryPercent }
                    ColumnLayout {
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 6.6
                        spacing: 0
                        PlasmaComponents.Label {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            font.weight: Font.Medium
                            font.pixelSize: Kirigami.Theme.defaultFont.pixelSize
                            text: Math.round(page.batteryPercent) + "% · " + page.batteryStateText()
                        }
                        PlasmaComponents.Label {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            visible: text !== ""
                            opacity: 0.65
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            text: page.durationText(page.batterySeconds)
                        }
                    }
                }
                TapHandler { onTapped: page.activateAppletById("org.kde.plasma.battery") }
            }

            Rectangle {
                visible: page.hasPerformanceProfiles
                Layout.fillWidth: true
                Layout.preferredHeight: Kirigami.Units.gridUnit * 2.75
                radius: height / 2
                color: Qt.rgba(1, 1, 1, 0.08)
                RowLayout {
                    anchors.centerIn: parent
                    spacing: 12
                    RowLayout {
                        spacing: Kirigami.Units.smallSpacing
                        Kirigami.Icon {
                            source: "speedometer-symbolic"
                            implicitWidth: Kirigami.Units.iconSizes.small
                            implicitHeight: implicitWidth
                        }
                        PlasmaComponents.Label {
                            text: i18n("Profile")
                            font.weight: Font.Medium
                            font.pixelSize: Kirigami.Theme.defaultFont.pixelSize
                        }
                    }
                    PlasmaComponents.ComboBox {
                        id: presetSelector
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 5.8
                        Layout.preferredHeight: Kirigami.Units.gridUnit * 1.4
                        model: page.performancePresets
                        flat: true
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        Kirigami.Theme.textColor: page.accentTextColor
                        rightPadding: 30
                        indicator: Canvas {
                            implicitWidth: 12
                            implicitHeight: 8
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            onPaint: {
                                const ctx = getContext("2d");
                                ctx.reset();
                                ctx.fillStyle = page.accentTextColor;
                                ctx.beginPath();
                                ctx.moveTo(1, 1);
                                ctx.lineTo(width - 1, 1);
                                ctx.lineTo(width / 2, height - 1);
                                ctx.closePath();
                                ctx.fill();
                            }
                            Connections {
                                target: page
                                function onAccentTextColorChanged() { parent.requestPaint(); }
                            }
                        }
                        background: Rectangle {
                            radius: height / 2
                            color: presetSelector.pressed ? Qt.darker(page.accentColor, 1.08) : page.accentColor
                        }
                        function syncSelection() {
                            const index = page.performancePresets.indexOf(page.activePerformancePreset);
                            if (index >= 0) currentIndex = index;
                        }
                        Component.onCompleted: syncSelection()
                        onActivated: index => page.applyPerformancePreset(page.performancePresets[index])
                        Connections {
                            target: page
                            function onActivePerformancePresetChanged() { presetSelector.syncSelection(); }
                            function onPerformancePresetsChanged() { presetSelector.syncSelection(); }
                        }
                    }
                }
            }
        }
    }
}
