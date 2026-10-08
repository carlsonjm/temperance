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

Item {
    id: page

    StatusColors {
        id: tone
        theme: page.Kirigami.Theme
    }

    required property var activateAppletById
    required property var controlCenterModel
    required property var performancePresets
    required property string activePerformancePreset
    required property var openPerformancePage
    required property color accentColor
    readonly property int compactSpacing: 8
    readonly property int standardSpacing: 12
    readonly property int surfaceSpacing: 24
    readonly property color accentTextColor: (accentColor.r * 0.299 + accentColor.g * 0.587 + accentColor.b * 0.114) > 0.58
        ? "#102729" : "#FFFFFF"

    implicitWidth: Kirigami.Units.gridUnit * 24
    // Match the Tray page's 12 px upper rhythm and 24 px pill-to-edge floor.
    implicitHeight: contentLayout.implicitHeight + standardSpacing + surfaceSpacing

    property string displayName: ""
    property bool sessionActionError: false
    Connections {
        target: Plasmoid
        function onSessionActionFailed() { page.sessionActionError = true }
    }

    function requestSessionAction(action) {
        page.sessionActionError = false
        Plasmoid.requestSessionAction(action)
    }
    property int displayBrightness: 0
    property int displayBrightnessMax: 100
    readonly property bool hasPerformanceProfiles: performancePresets.length > 0

    // The Performance pill takes its place in Control Center's order from
    // the same list as the tray entries; unlisted, it comes last.
    readonly property string performanceItemId: "temperance.performance"
    readonly property int performanceOrder: {
        const position = (Plasmoid.configuration.controlCenterItems || []).indexOf(performanceItemId);
        return position >= 0 ? position : 100;
    }
    property int controlOrderRevision: 0
    readonly property int performanceSlot: {
        controlOrderRevision;
        const model = page.controlCenterModel;
        const role = model.KItemModels.KRoleNames.role("controlCenterOrder");
        let slot = 0;
        for (let row = 0; row < model.rowCount(); ++row) {
            if (model.data(model.index(row, 0), role) < performanceOrder) ++slot;
        }
        return slot;
    }

    Connections {
        target: page.controlCenterModel
        function onRowsInserted() { page.controlOrderRevision++; }
        function onRowsRemoved() { page.controlOrderRevision++; }
        function onRowsMoved() { page.controlOrderRevision++; }
        function onModelReset() { page.controlOrderRevision++; }
        function onLayoutChanged() { page.controlOrderRevision++; }
        function onDataChanged() { page.controlOrderRevision++; }
    }

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
        color: actionHover.hovered ? tone.wash(0.12) : tone.wash(0.07)
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
        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: tileText
        Accessible.onPressAction: triggered()
        Keys.onReturnPressed: triggered()
        Keys.onSpacePressed: triggered()
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
                color: tone.wash(0.07)
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
        readonly property color ink: tone.text
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
        // On the popup's margin line, 12 px into the page.
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: page.standardSpacing
        spacing: 0

        PlasmaComponents.Label {
            visible: page.sessionActionError
            Layout.fillWidth: true
            Layout.bottomMargin: visible ? page.standardSpacing : 0
            text: i18n("The session action is unavailable or could not be started. Please try again.")
            wrapMode: Text.Wrap
        }

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
                font.features: { "tnum": 1 }
            }
        }

        // Volume and brightness are one group, nearer each other than the
        // tiles below them.
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: page.compactSpacing
            spacing: Kirigami.Units.smallSpacing
            PlasmaComponents.ToolButton {
                icon.source: "qrc:/qt/qml/plasma/applet/studio/warbler/temperance/sun.svg"
                icon.color: tone.text
                contentItem: SuiteIcon {
                    glyph: "sun"
                    implicitWidth: 20; implicitHeight: 20
                }
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
                font.features: { "tnum": 1 }
            }
        }

        GridLayout {
            id: controlGrid
            readonly property int visibleRows: Math.ceil(
                (controlRepeater.count + (page.hasPerformanceProfiles ? 1 : 0)) / columns)
            readonly property real tileHeight: Kirigami.Units.gridUnit * 2.75
            implicitHeight: visibleRows > 0
                ? visibleRows * tileHeight + (visibleRows - 1) * rowSpacing : 0
            Layout.fillWidth: true
            Layout.topMargin: visibleRows > 0 ? page.surfaceSpacing : 0
            Layout.preferredHeight: implicitHeight
            columns: 2
            columnSpacing: page.compactSpacing
            rowSpacing: page.compactSpacing

            Repeater {
                id: controlRepeater
                model: page.controlCenterModel
                delegate: Item {
                    id: controlItem
                    required property int index
                    required property int effectiveStatus
                    required property var model
                    readonly property int slot: index
                        + (page.hasPerformanceProfiles && index >= page.performanceSlot ? 1 : 0)
                    Layout.row: Math.floor(slot / controlGrid.columns)
                    Layout.column: slot % controlGrid.columns
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

            // The profile in use, drawn as a pill among the others and
            // ordered with them; a tap opens the Performance page.
            Rectangle {
                id: performanceEntry
                objectName: "temperance-performance-entry"
                visible: page.hasPerformanceProfiles
                Layout.row: Math.floor(page.performanceSlot / controlGrid.columns)
                Layout.column: page.performanceSlot % controlGrid.columns
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 0
                Layout.preferredHeight: controlGrid.tileHeight
                radius: height / 2
                color: performanceTap.pressed || performanceHover.hovered
                    ? tone.wash(0.12) : tone.wash(0.07)
                RowLayout {
                    anchors.centerIn: parent
                    width: Math.min(implicitWidth, performanceEntry.width - Kirigami.Units.largeSpacing * 2)
                    spacing: 8
                    SuiteIcon {
                        glyph: "gauge"
                        implicitWidth: Kirigami.Units.iconSizes.smallMedium
                        implicitHeight: implicitWidth
                    }
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: page.activePerformancePreset || i18n("Performance")
                        maximumLineCount: 1
                        elide: Text.ElideRight
                    }
                }
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: i18n("Performance")
                Accessible.description: page.activePerformancePreset
                Accessible.onPressAction: page.openPerformancePage()
                Keys.onReturnPressed: page.openPerformancePage()
                Keys.onSpacePressed: page.openPerformancePage()
                HoverHandler { id: performanceHover }
                TapHandler { id: performanceTap; onTapped: page.openPerformancePage() }
            }
        }
    }
}
