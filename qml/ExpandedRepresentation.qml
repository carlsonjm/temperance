/*
    SPDX-FileCopyrightText: 2016 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2020 Nate Graham <nate@kde.org>
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami
import org.kde.notificationmanager as NotificationManager
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.plasma.plasmoid

Item {
    id: popup

    readonly property int nativePagePadding: 12
    readonly property int contentSafety: 4
    readonly property int headerSafety: 4
    readonly property real desiredWidth: Kirigami.Units.gridUnit * 24
        + (systemTrayState.activeApplet ? nativePagePadding * 2 : 0)
        + contentSafety * 2
    readonly property real desiredHeight: {
        if (systemTrayState.activeApplet) {
            return Kirigami.Units.gridUnit * 22 + nativePagePadding * 2
                + contentSafety * 2 + headerSafety;
        }
        const pageHeight = systemTrayState.page === "control" ? controlPage.implicitHeight
            : systemTrayState.page === "notifications" ? notificationPage.implicitHeight
            : organizedPage.implicitHeight;
        return pageHeight + heading.implicitHeight + headingBackground.bottomPadding
            + contentSafety * 2 + headerSafety;
    }
    implicitWidth: desiredWidth
    implicitHeight: desiredHeight
    Layout.minimumWidth: desiredWidth
    Layout.preferredWidth: desiredWidth
    Layout.maximumWidth: desiredWidth
    Layout.minimumHeight: desiredHeight
    Layout.preferredHeight: desiredHeight
    Layout.maximumHeight: desiredHeight

    property real presentationOffset: 24
    property real presentationOpacity: 0
    opacity: presentationOpacity
    scale: 0.985 + (1 - Math.min(1, presentationOffset / 24)) * 0.015
    transformOrigin: Item.Bottom
    transform: Translate { y: popup.presentationOffset }

    property alias hiddenLayout: compatibilityLayout
    property alias plasmoidContainer: container

    function playEntrance() {
        entranceMotion.stop();
        presentationOffset = 24;
        presentationOpacity = 0;
        entranceMotion.restart();
    }

    Component.onCompleted: {
        if (systemTrayState.expanded) Qt.callLater(playEntrance);
    }

    Connections {
        target: systemTrayState
        function onExpandedChanged() {
            if (systemTrayState.expanded) Qt.callLater(popup.playEntrance);
        }
    }

    ParallelAnimation {
        id: entranceMotion
        SpringAnimation {
            target: popup
            property: "presentationOffset"
            to: 0
            spring: 4.2
            damping: 0.48
            epsilon: 0.08
        }
        NumberAnimation {
            target: popup
            property: "presentationOpacity"
            from: 0
            to: 1
            duration: 190
            easing.type: Easing.OutCubic
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#141414"
        radius: 18
        border.width: 1
        border.color: "#333333"
        z: -10000
    }

    function pageTitle() {
        if (systemTrayState.activeApplet) return systemTrayState.activeApplet.plasmoid.title;
        if (systemTrayState.page === "tray") return i18n("System Tray");
        if (systemTrayState.page === "notifications") return i18n("Notifications");
        return i18n("Control Center");
    }

    PlasmaExtras.PlasmoidHeading {
        id: headingBackground
        opacity: 0
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: heading.height + bottomPadding + container.headingHeight
        Behavior on height {
            NumberAnimation { duration: Kirigami.Units.shortDuration / 2; easing.type: Easing.InOutQuad }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: popup.contentSafety
        spacing: headingBackground.bottomPadding

        RowLayout {
            id: heading
            Layout.fillWidth: true
            Layout.leftMargin: popup.headerSafety
            Layout.rightMargin: popup.headerSafety
            Layout.topMargin: popup.headerSafety

            PlasmaComponents.ToolButton {
                visible: systemTrayState.activeApplet !== null || systemTrayState.page === "notifications"
                icon.name: "go-previous-symbolic"
                display: PlasmaComponents.AbstractButton.IconOnly
                text: i18n("Back")
                onClicked: {
                    systemTrayState.setActiveApplet(null);
                    systemTrayState.page = systemTrayState.page === "notifications" ? "tray" : "control";
                }
            }

            Kirigami.Heading {
                Layout.fillWidth: true
                leftPadding: systemTrayState.activeApplet ? 0 : Kirigami.Units.largeSpacing
                level: 1
                text: popup.pageTitle()
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            PlasmaComponents.ToolButton {
                visible: systemTrayState.page === "control" && !systemTrayState.activeApplet
                icon.name: "applications-utilities-symbolic"
                display: PlasmaComponents.AbstractButton.IconOnly
                text: i18n("Open System Settings")
                onClicked: {
                    systemTrayState.expanded = false;
                    KCM.KCMLauncher.openSystemSettings("kcm_landingpage");
                }
                PlasmaComponents.ToolTip { text: parent.text }
            }

            PlasmaComponents.ToolButton {
                visible: systemTrayState.page === "tray" && !systemTrayState.activeApplet
                icon.name: "configure-symbolic"
                display: PlasmaComponents.AbstractButton.IconOnly
                text: i18n("Configure system tray icons")
                onClicked: Plasmoid.internalAction("configure").trigger()
                PlasmaComponents.ToolTip { text: parent.text }
            }

            PlasmaComponents.ToolButton {
                readonly property bool isWeather: systemTrayState.activeApplet
                    && systemTrayState.activeApplet.Plasmoid.pluginName === "org.kde.plasma.weather"
                visible: isWeather && String(Plasmoid.configuration.weatherApplication || "").length > 0
                icon.name: "weather-clear-symbolic"
                display: PlasmaComponents.AbstractButton.IconOnly
                text: i18n("Open Weather")
                onClicked: {
                    systemTrayState.expanded = false;
                    Plasmoid.launchApplication(Plasmoid.configuration.weatherApplication);
                }
                PlasmaComponents.ToolTip { text: parent.text }
            }

            RowLayout {
                id: doNotDisturbControl
                visible: systemTrayState.page === "notifications" && !systemTrayState.activeApplet
                spacing: 7

                PlasmaComponents.Label {
                    text: i18n("Do Not Disturb")
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                    font.weight: Font.Medium
                    color: "#F8F8FF"
                    opacity: doNotDisturbPill.checked ? 1 : 0.78
                    Behavior on opacity { NumberAnimation { duration: 140 } }
                }

                Rectangle {
                    id: doNotDisturbPill
                    readonly property bool checked: NotificationManager.Server.inhibited
                    Layout.preferredWidth: 42
                    Layout.minimumWidth: 42
                    Layout.maximumWidth: 42
                    Layout.preferredHeight: 30
                    Layout.maximumHeight: 30
                    radius: height / 2
                    clip: true
                    color: checked ? root.accentColor
                        : dndHover.hovered ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, 0.07)
                    border.width: checked ? 0 : 1
                    border.color: "#333333"
                    Behavior on color { ColorAnimation { duration: 140; easing.type: Easing.OutCubic } }

                    BellGlyph {
                        width: 18
                        height: 18
                        anchors.centerIn: parent
                        glyphColor: doNotDisturbPill.checked ? "#102729" : "#F8F8FF"
                        slashed: doNotDisturbPill.checked
                        strokeWidth: 1.55
                    }

                    HoverHandler { id: dndHover }
                    TapHandler {
                        onTapped: NotificationManager.Server.inhibited = !NotificationManager.Server.inhibited
                    }
                    PlasmaComponents.ToolTip {
                        text: doNotDisturbPill.checked
                            ? i18n("Turn off Do Not Disturb") : i18n("Turn on Do Not Disturb")
                    }
                }
            }

            PlasmaComponents.ToolButton {
                visible: Plasmoid.configuration.showPinButton
                checkable: true
                checked: Plasmoid.configuration.pin
                onToggled: Plasmoid.configuration.pin = checked
                icon.name: "window-pin"
                display: PlasmaComponents.AbstractButton.IconOnly
                text: i18n("Keep Open")
                PlasmaComponents.ToolTip { text: parent.text }
            }
        }

        ControlCenterPage {
            id: controlPage
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !systemTrayState.activeApplet && systemTrayState.page === "control"
            activateAppletById: itemId => root.activateAppletById(itemId)
            controlCenterModel: root.controlCenterModel
            performancePresets: Plasmoid.performancePresets
            activePerformancePreset: Plasmoid.activePerformancePreset
            applyPerformancePreset: name => Plasmoid.applyPerformancePreset(name)
            accentColor: root.accentColor
        }

        OrganizedTrayPage {
            id: organizedPage
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !systemTrayState.activeApplet && systemTrayState.page === "tray"
        }

        NotificationHistoryPage {
            id: notificationPage
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !systemTrayState.activeApplet && systemTrayState.page === "notifications"
            notificationModel: root.notificationHistoryModel
            demoNotificationVisible: root.demoNotificationVisible
            clearHistory: () => root.clearNotificationHistory()
            resolveApplicationIcon: (applicationName, desktopEntry, fallbackIcon) =>
                Plasmoid.resolveApplicationIcon(applicationName, desktopEntry, fallbackIcon)
        }

        PlasmoidPopupsContainer {
            id: container
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: systemTrayState.activeApplet !== null
            Layout.topMargin: mergeHeadings ? 0 : dialog.topPadding
            Layout.leftMargin: popup.nativePagePadding
            Layout.rightMargin: popup.nativePagePadding
            Layout.bottomMargin: popup.nativePagePadding
        }

        // Inherited item delegates use this cursor for hover bookkeeping. The
        // categorized page owns the actual visible layout.
        Item {
            id: compatibilityLayout
            property int currentIndex: -1
            visible: false
            Layout.preferredHeight: 0
            Layout.preferredWidth: 0
        }
    }

    PlasmaExtras.PlasmoidHeading {
        position: PlasmaComponents.ToolBar.Footer
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
        visible: container.appletHasFooter
        height: container.footerHeight
        z: -9999
        opacity: 0
    }
}
