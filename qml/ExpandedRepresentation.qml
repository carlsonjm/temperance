/*
    SPDX-FileCopyrightText: 2016 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2020 Nate Graham <nate@kde.org>
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
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
    readonly property real notificationPageHeightLimit: Math.max(1,
        root.notificationPopupHeightLimit - heading.implicitHeight
        - headingBackground.bottomPadding - contentSafety * 2 - headerSafety)
    readonly property bool calendarShown: !systemTrayState.activeApplet
        && systemTrayState.page === "calendar"
    readonly property real desiredWidth: calendarShown
        ? calendarPage.implicitWidth + contentSafety * 2
        : Kirigami.Units.gridUnit * 24
            + (systemTrayState.activeApplet ? nativePagePadding * 2 : 0)
            + contentSafety * 2
    readonly property real desiredHeight: {
        if (systemTrayState.activeApplet) {
            return Kirigami.Units.gridUnit * 22 + nativePagePadding * 2
                + contentSafety * 2 + headerSafety;
        }
        const pageHeight = systemTrayState.page === "control" ? controlPage.implicitHeight
            : systemTrayState.page === "notifications" ? notificationPage.implicitHeight
            : systemTrayState.page === "calendar" ? calendarPage.implicitHeight
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
        if (Kirigami.Units.longDuration <= 0) {
            presentationOffset = 0;
            presentationOpacity = 1;
            return;
        }
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
            if (systemTrayState.expanded && systemTrayState.page === "calendar") calendarPage.reset();
        }
        function onPageChanged() {
            if (systemTrayState.expanded && systemTrayState.page === "calendar") calendarPage.reset();
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
            duration: Kirigami.Units.longDuration
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
        if (systemTrayState.page === "tray") return i18n("System tray");
        if (systemTrayState.page === "notifications") return i18n("Notifications & events");
        if (systemTrayState.page === "calendar") return calendarPage.title;
        return i18n("Control center");
    }

    component HeaderToolTip: PlasmaComponents.ToolTip {
        // Header controls sit at the window edge: keep labels inside the surface.
        readonly property real anchorX: parent ? parent.mapToItem(popup, 0, 0).x : 0
        y: parent ? parent.height + 4 : 0
        x: parent ? Math.max(10 - anchorX,
            Math.min((parent.width - implicitWidth) / 2,
                popup.width - 10 - anchorX - implicitWidth)) : 0
    }

    // The calendar's month arrows: no boundary at rest, since the chevron is
    // the whole affordance, and the header family's 30 px height and radius.
    component CalendarArrow: PlasmaComponents.ToolButton {
        id: arrow
        required property string glyph
        Layout.preferredWidth: 30
        Layout.preferredHeight: 30
        padding: 6
        display: PlasmaComponents.AbstractButton.IconOnly
        Accessible.name: text
        contentItem: SuiteIcon {
            glyph: arrow.glyph
            implicitWidth: 18
            implicitHeight: 18
        }
        background: Rectangle {
            radius: height / 2
            color: arrow.down ? Qt.rgba(1, 1, 1, 0.18)
                : arrow.hovered ? Qt.rgba(1, 1, 1, 0.12) : "transparent"
            border.width: arrow.visualFocus ? 1 : 0
            border.color: "#F8F8FF"
            Behavior on color { ColorAnimation { duration: 120; easing.type: Easing.OutCubic } }
        }
        HeaderToolTip { text: arrow.text }
    }

    component HeaderPowerPill: PlasmaComponents.ToolButton {
        id: powerPill
        required property string glyph
        Layout.preferredWidth: 42
        Layout.minimumWidth: 42
        Layout.maximumWidth: 42
        Layout.preferredHeight: 30
        Layout.minimumHeight: 30
        Layout.maximumHeight: 30
        leftPadding: 12
        rightPadding: 12
        topPadding: 6
        bottomPadding: 6
        Accessible.name: text
        contentItem: SuiteIcon {
            glyph: powerPill.glyph
            // ToolButton expands its contentItem to the available 18 px box.
            // Inset the rendered image so the visible glyph is truly 14 px.
            glyphInset: 2
            implicitWidth: 18
            implicitHeight: 18
        }
        background: Rectangle {
            radius: height / 2
            color: powerPill.hovered || powerPill.down ? Qt.rgba(1, 1, 1, 0.12) : "transparent"
            border.width: 1
            border.color: powerPill.activeFocus ? "#F8F8FF" : "#5a5a5a"
            Behavior on color { ColorAnimation { duration: 120 } }
        }
        HeaderToolTip { text: powerPill.text }
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
            // On the calendar the arrows end on the grid's 16 px margin line.
            Layout.rightMargin: popup.calendarShown ? calendarPage.textInset : popup.headerSafety
            Layout.topMargin: popup.headerSafety

            Kirigami.Heading {
                Layout.fillWidth: true
                leftPadding: Kirigami.Units.largeSpacing
                level: 1
                text: popup.pageTitle()
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            RowLayout {
                id: calendarNavigation
                visible: popup.calendarShown
                spacing: 8

                PlasmaComponents.ToolButton {
                    id: calendarToday
                    visible: !calendarPage.showingCurrentMonth
                    Layout.preferredHeight: 30
                    leftPadding: 12
                    rightPadding: 12
                    text: i18nc("@action:button go to the current month", "Today")
                    contentItem: PlasmaComponents.Label {
                        text: calendarToday.text
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        color: "#F8F8FF"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: height / 2
                        color: calendarToday.down ? Qt.rgba(1, 1, 1, 0.18)
                            : calendarToday.hovered ? Qt.rgba(1, 1, 1, 0.12) : "transparent"
                        border.width: 1
                        border.color: calendarToday.visualFocus ? "#F8F8FF" : "#5a5a5a"
                        Behavior on color { ColorAnimation { duration: 120; easing.type: Easing.OutCubic } }
                    }
                    onClicked: calendarPage.reset()
                }

                RowLayout {
                    spacing: 4
                    CalendarArrow {
                        glyph: "chevron-left"
                        text: i18n("Previous month")
                        onClicked: calendarPage.showMonth(-1)
                    }
                    CalendarArrow {
                        glyph: "chevron-right"
                        text: i18n("Next month")
                        onClicked: calendarPage.showMonth(1)
                    }
                }
            }

            RowLayout {
                id: sessionActions
                visible: systemTrayState.page === "control" && !systemTrayState.activeApplet
                    && (Plasmoid.configuration.showRestart || Plasmoid.configuration.showShutdown || Plasmoid.configuration.showLogout || Plasmoid.configuration.showSwitchUser)
                spacing: 4
                HeaderPowerPill {
                    visible: Plasmoid.configuration.showLogout
                    glyph: "log-out"
                    text: i18n("Log out")
                    onClicked: controlPage.requestSessionAction("logout")
                }
                HeaderPowerPill {
                    visible: Plasmoid.configuration.showRestart
                    glyph: "rotate-cw"
                    text: i18n("Restart")
                    onClicked: controlPage.requestSessionAction("restart")
                }
                HeaderPowerPill {
                    visible: Plasmoid.configuration.showShutdown
                    glyph: "power"
                    text: i18n("Shut down")
                    onClicked: controlPage.requestSessionAction("shutdown")
                }
                PlasmaComponents.ToolButton {
                    id: sessionMore
                    visible: Plasmoid.configuration.showSwitchUser
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 30
                    icon.source: "qrc:/qt/qml/plasma/applet/studio/warbler/temperance/ellipsis.svg"
                    icon.color: "#F8F8FF"
                    contentItem: SuiteIcon {
                        glyph: "ellipsis"
                        implicitWidth: 18; implicitHeight: 18
                    }
                    display: PlasmaComponents.AbstractButton.IconOnly
                    text: i18n("More session options")
                    onClicked: sessionMenu.open()
                    HeaderToolTip { text: parent.text }
                    QQC2.Menu {
                        id: sessionMenu
                        y: sessionMore.height + 4
                        x: sessionMore.width - width
                        width: 150
                        padding: 6
                        closePolicy: QQC2.Popup.CloseOnEscape | QQC2.Popup.CloseOnPressOutside
                        background: Rectangle {
                            color: "#141414"
                            radius: 14
                            border.width: 1
                            border.color: "#5a5a5a"
                        }
                        QQC2.MenuItem {
                            id: switchUserOption
                            text: i18n("Switch user")
                            implicitHeight: 36
                            onTriggered: controlPage.requestSessionAction("switchUser")
                            contentItem: PlasmaComponents.Label {
                                text: switchUserOption.text
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                radius: height / 2
                                color: switchUserOption.highlighted ? Qt.rgba(1, 1, 1, 0.12) : "transparent"
                            }
                        }
                    }
                }
                onVisibleChanged: if (!visible) sessionMenu.close()
            }

            PlasmaComponents.ToolButton {
                visible: systemTrayState.page === "tray" && !systemTrayState.activeApplet
                icon.source: "qrc:/qt/qml/plasma/applet/studio/warbler/temperance/settings.svg"
                icon.color: "#F8F8FF"
                contentItem: SuiteIcon {
                    glyph: "settings"
                    implicitWidth: 20; implicitHeight: 20
                }
                display: PlasmaComponents.AbstractButton.IconOnly
                text: i18n("Open system settings")
                onClicked: {
                    systemTrayState.expanded = false;
                    KCM.KCMLauncher.openSystemSettings("kcm_landingpage");
                }
                HeaderToolTip { text: parent.text }
            }

            PlasmaComponents.ToolButton {
                visible: systemTrayState.page === "tray" && !systemTrayState.activeApplet
                icon.source: "qrc:/qt/qml/plasma/applet/studio/warbler/temperance/sliders-horizontal.svg"
                icon.color: "#F8F8FF"
                contentItem: SuiteIcon {
                    glyph: "sliders-horizontal"
                    implicitWidth: 20; implicitHeight: 20
                }
                display: PlasmaComponents.AbstractButton.IconOnly
                text: i18n("Configure system tray icons")
                onClicked: Plasmoid.internalAction("configure").trigger()
                HeaderToolTip { text: parent.text }
            }

            PlasmaComponents.ToolButton {
                id: addBluetoothDevice
                objectName: "addBluetoothDevice"
                visible: systemTrayState.activeApplet
                    && systemTrayState.activeApplet.Plasmoid.pluginName === "org.kde.plasma.bluetooth"
                text: i18n("Add new device")
                icon.source: "qrc:/qt/qml/plasma/applet/studio/warbler/temperance/plus.svg"
                icon.color: "#F8F8FF"
                contentItem: Row {
                    spacing: 6
                    SuiteIcon {
                        glyph: "plus"
                        width: 18; height: 18
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    PlasmaComponents.Label {
                        text: addBluetoothDevice.text
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                Layout.minimumHeight: 44
                leftPadding: 12
                rightPadding: 12
                background: Item {
                    Rectangle {
                        objectName: "bluetoothPairingPill"
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width
                        height: 30
                        radius: height / 2
                        color: addBluetoothDevice.hovered || addBluetoothDevice.down
                            ? Qt.rgba(1, 1, 1, 0.12) : "transparent"
                        border.width: 1
                        border.color: addBluetoothDevice.activeFocus ? "#F8F8FF" : "#5a5a5a"
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }
                }
                onClicked: {
                    Plasmoid.launchApplication("org.kde.bluedevilwizard");
                    systemTrayState.expanded = false;
                }
                HeaderToolTip { text: parent.text }
            }

            PlasmaComponents.ToolButton {
                readonly property bool isWeather: systemTrayState.activeApplet
                    && systemTrayState.activeApplet.Plasmoid.pluginName === "org.kde.plasma.weather"
                visible: isWeather && String(Plasmoid.configuration.weatherApplication || "").length > 0
                icon.name: "weather-clear-symbolic"
                display: PlasmaComponents.AbstractButton.IconOnly
                text: i18n("Open weather")
                onClicked: {
                    systemTrayState.expanded = false;
                    Plasmoid.launchApplication(Plasmoid.configuration.weatherApplication);
                }
                HeaderToolTip { text: parent.text }
            }

            RowLayout {
                id: doNotDisturbControl
                visible: systemTrayState.page === "notifications" && !systemTrayState.activeApplet
                spacing: 7

                PlasmaComponents.Label {
                    text: i18n("do not disturb")
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
                    HeaderToolTip {
                        text: doNotDisturbPill.checked
                            ? i18n("Turn off do not disturb") : i18n("Turn on do not disturb")
                    }
                }
            }

            PlasmaComponents.ToolButton {
                visible: Plasmoid.configuration.showPinButton
                checkable: true
                checked: Plasmoid.configuration.pin
                onToggled: Plasmoid.configuration.pin = checked
                icon.source: "qrc:/qt/qml/plasma/applet/studio/warbler/temperance/pin.svg"
                icon.color: "#F8F8FF"
                contentItem: SuiteIcon {
                    glyph: "pin"
                    implicitWidth: 20; implicitHeight: 20
                }
                display: PlasmaComponents.AbstractButton.IconOnly
                text: i18n("Keep open")
                HeaderToolTip { text: parent.text }
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

        CalendarPage {
            id: calendarPage
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: popup.calendarShown
            today: systemClock.dateTime
            accentColor: root.accentColor
            feeds: Plasmoid.calendarFeeds
            formatTime: value => statusClock.timeString(value)
        }

        NotificationHistoryPage {
            id: notificationPage
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !systemTrayState.activeApplet && systemTrayState.page === "notifications"
            notificationModel: root.notificationHistoryModel
            maximumHeight: popup.notificationPageHeightLimit
            launchApplication: desktopEntry => Plasmoid.launchApplication(desktopEntry)
            demoNotificationVisible: root.demoNotificationVisible
            clearHistory: () => root.clearNotificationHistory()
            resolveApplicationIcon: (applicationName, desktopEntry, fallbackIcon) =>
                Plasmoid.resolveApplicationIcon(applicationName, desktopEntry, fallbackIcon)
            events: root.todayEvents
            dismissEvent: key => root.dismissEvent(key)
            formatTime: value => statusClock.timeString(value)
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
