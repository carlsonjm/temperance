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
    // Every page lines up on one margin line 16 px in from each edge: the
    // title, each page's content, and the header's controls, whose last one
    // ends on it. Pages sit 4 px in, so they place content 12 px in.
    readonly property int marginLine: 16
    readonly property int pageInset: marginLine - contentSafety
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
        // Shuffle's note corner: it floats above everything and closes.
        radius: 12
        border.width: 1
        border.color: "#333333"
        z: -10000
    }

    function pageTitle() {
        if (systemTrayState.activeApplet) return systemTrayState.activeApplet.plasmoid.title;
        if (systemTrayState.page === "tray") return i18n("System Tray");
        if (systemTrayState.page === "notifications") return i18n("Notifications & Events");
        if (systemTrayState.page === "calendar") return calendarPage.title;
        return i18n("Control Center");
    }

    component HeaderToolTip: PlasmaComponents.ToolTip {
        // Header controls sit at the window edge: keep labels inside the surface.
        readonly property real anchorX: parent ? parent.mapToItem(popup, 0, 0).x : 0
        y: parent ? parent.height + 4 : 0
        x: parent ? Math.max(10 - anchorX,
            Math.min((parent.width - implicitWidth) / 2,
                popup.width - 10 - anchorX - implicitWidth)) : 0
    }

    // Plasma's buttons stop hearing the pointer in tablet mode, though a mouse
    // or a trackpad may still be in use, so the header's own buttons watch for
    // one themselves. A finger is left out: a tap would leave a hover behind.
    component PointerHover: HoverHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.Stylus
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
                : arrowHover.hovered ? Qt.rgba(1, 1, 1, 0.12) : "transparent"
            border.width: arrow.visualFocus ? 1 : 0
            border.color: "#F8F8FF"
            Behavior on color { ColorAnimation { duration: 120; easing.type: Easing.OutCubic } }
        }
        PointerHover { id: arrowHover }
        HeaderToolTip { text: arrow.text }
    }

    // The header's button: a grey pill as large as its 42 x 30 touch, with a
    // 14 px glyph and no outline, lighter under the pointer, lighter again
    // pressed or on. Every header control but the calendar's takes this grey.
    component HeaderPill: PlasmaComponents.ToolButton {
        id: powerPill
        required property string glyph
        objectName: "temperance-header-" + glyph
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
            color: powerPill.down || powerPill.checked ? "#4A4A4A"
                : pillHover.hovered ? "#333333" : "#242424"
            border.width: powerPill.visualFocus ? 1 : 0
            border.color: "#F8F8FF"
            Behavior on color { ColorAnimation { duration: 120 } }
        }
        PointerHover { id: pillHover }
        HeaderToolTip { text: powerPill.text }
    }

    // The header's grey as a 30 px circle, for what moves around the header
    // rather than acting: back, and the session menu.
    component HeaderCircle: PlasmaComponents.ToolButton {
        id: headerCircle
        required property string glyph
        // Held lit while what it opened is open.
        property bool held: false
        Layout.preferredWidth: 30
        Layout.preferredHeight: 30
        Layout.maximumWidth: 30
        Layout.maximumHeight: 30
        padding: 6
        display: PlasmaComponents.AbstractButton.IconOnly
        Accessible.name: text
        contentItem: SuiteIcon {
            glyph: headerCircle.glyph
            glyphInset: 1
            implicitWidth: 18
            implicitHeight: 18
        }
        background: Rectangle {
            radius: height / 2
            color: headerCircle.down || headerCircle.held ? "#4A4A4A"
                : circleHover.hovered ? "#333333" : "#242424"
            border.width: headerCircle.visualFocus ? 1 : 0
            border.color: "#F8F8FF"
            Behavior on color { ColorAnimation { duration: 120 } }
        }
        PointerHover { id: circleHover }
        HeaderToolTip { text: headerCircle.text }
    }

    // A line of the session menu; hidden, it takes no room.
    component SessionMenuItem: QQC2.MenuItem {
        id: sessionOption
        implicitHeight: visible ? 36 : 0
        contentItem: PlasmaComponents.Label {
            text: sessionOption.text
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: height / 2
            color: sessionOption.highlighted ? Qt.rgba(1, 1, 1, 0.12) : "transparent"
        }
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
            Layout.rightMargin: popup.pageInset
            Layout.topMargin: popup.headerSafety

            // A page an applet shows, Networks say, goes back to the page it
            // was opened from.
            HeaderCircle {
                objectName: "temperance-header-back"
                visible: systemTrayState.activeApplet !== null
                glyph: "chevron-left"
                text: i18n("Back")
                onClicked: systemTrayState.setActiveApplet(null)
            }

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
                            : todayHover.hovered ? Qt.rgba(1, 1, 1, 0.12) : "transparent"
                        border.width: 1
                        border.color: calendarToday.visualFocus ? "#F8F8FF" : "#5a5a5a"
                        Behavior on color { ColorAnimation { duration: 120; easing.type: Easing.OutCubic } }
                    }
                    onClicked: calendarPage.reset()
                    PointerHover { id: todayHover }
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
                    && (Plasmoid.configuration.showLock || Plasmoid.configuration.showRestart || Plasmoid.configuration.showShutdown || Plasmoid.configuration.showLogout || Plasmoid.configuration.showSwitchUser)
                spacing: 4
                // The device's own actions stand in the row; the session's,
                // log out and switch user, wait behind the ellipsis.
                HeaderPill {
                    visible: Plasmoid.configuration.showLock
                    glyph: "lock"
                    text: i18n("Lock")
                    onClicked: controlPage.requestSessionAction("lock")
                }
                HeaderPill {
                    visible: Plasmoid.configuration.showRestart
                    glyph: "rotate-cw"
                    text: i18n("Restart")
                    onClicked: controlPage.requestSessionAction("restart")
                }
                HeaderPill {
                    visible: Plasmoid.configuration.showShutdown
                    glyph: "power"
                    text: i18n("Shut down")
                    onClicked: controlPage.requestSessionAction("shutdown")
                }
                HeaderCircle {
                    id: sessionMore
                    objectName: "temperance-session-more"
                    visible: Plasmoid.configuration.showLogout || Plasmoid.configuration.showSwitchUser
                    glyph: "ellipsis-vertical"
                    icon.source: "qrc:/qt/qml/plasma/applet/studio/warbler/temperance/ellipsis-vertical.svg"
                    held: sessionMenu.visible
                    text: i18n("More session options")
                    onClicked: sessionMenu.open()
                    QQC2.Menu {
                        id: sessionMenu
                        y: sessionMore.height + 4
                        x: sessionMore.width - width
                        width: 150
                        padding: 6
                        closePolicy: QQC2.Popup.CloseOnEscape | QQC2.Popup.CloseOnPressOutside
                        background: Rectangle {
                            color: "#141414"
                            radius: 12
                            border.width: 1
                            border.color: "#5a5a5a"
                        }
                        SessionMenuItem {
                            visible: Plasmoid.configuration.showLogout
                            text: i18n("Log out")
                            onTriggered: controlPage.requestSessionAction("logout")
                        }
                        SessionMenuItem {
                            visible: Plasmoid.configuration.showSwitchUser
                            text: i18n("Switch user")
                            onTriggered: controlPage.requestSessionAction("switchUser")
                        }
                    }
                }
                onVisibleChanged: if (!visible) sessionMenu.close()
            }

            HeaderPill {
                visible: systemTrayState.page === "tray" && !systemTrayState.activeApplet
                glyph: "settings"
                icon.source: "qrc:/qt/qml/plasma/applet/studio/warbler/temperance/settings.svg"
                text: i18n("Open system settings")
                onClicked: {
                    systemTrayState.expanded = false;
                    KCM.KCMLauncher.openSystemSettings("kcm_landingpage");
                }
            }

            HeaderPill {
                visible: systemTrayState.page === "tray" && !systemTrayState.activeApplet
                glyph: "sliders-horizontal"
                text: i18n("Configure system tray icons")
                onClicked: Plasmoid.internalAction("configure").trigger()
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
                        color: addBluetoothDevice.down ? "#4A4A4A"
                            : addBluetoothHover.hovered ? "#333333" : "#242424"
                        border.width: addBluetoothDevice.visualFocus ? 1 : 0
                        border.color: "#F8F8FF"
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }
                }
                onClicked: {
                    Plasmoid.launchApplication("org.kde.bluedevilwizard");
                    systemTrayState.expanded = false;
                }
                PointerHover { id: addBluetoothHover }
                HeaderToolTip { text: parent.text }
            }

            HeaderPill {
                readonly property bool isWeather: systemTrayState.activeApplet
                    && systemTrayState.activeApplet.Plasmoid.pluginName === "org.kde.plasma.weather"
                visible: isWeather && String(Plasmoid.configuration.weatherApplication || "").length > 0
                glyph: "cloud-sun"
                text: i18n("Open weather")
                onClicked: {
                    systemTrayState.expanded = false;
                    Plasmoid.launchApplication(Plasmoid.configuration.weatherApplication);
                }
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
                    objectName: "temperance-do-not-disturb"
                    readonly property bool checked: NotificationManager.Server.inhibited
                    Layout.preferredWidth: 42
                    Layout.minimumWidth: 42
                    Layout.maximumWidth: 42
                    Layout.preferredHeight: 30
                    Layout.maximumHeight: 30
                    radius: height / 2
                    clip: true
                    color: checked ? root.accentColor
                        : dndTap.pressed ? "#4A4A4A" : dndHover.hovered ? "#333333" : "#242424"
                    Behavior on color { ColorAnimation { duration: 140; easing.type: Easing.OutCubic } }

                    BellGlyph {
                        width: 18
                        height: 18
                        anchors.centerIn: parent
                        glyphColor: doNotDisturbPill.checked ? "#102729" : "#F8F8FF"
                        slashed: doNotDisturbPill.checked
                        strokeWidth: 1.55
                    }

                    PointerHover { id: dndHover }
                    TapHandler {
                        id: dndTap
                        onTapped: NotificationManager.Server.inhibited = !NotificationManager.Server.inhibited
                    }
                    HeaderToolTip {
                        text: doNotDisturbPill.checked
                            ? i18n("Turn off do not disturb") : i18n("Turn on do not disturb")
                    }
                }
            }

            HeaderPill {
                visible: Plasmoid.configuration.showPinButton
                checkable: true
                checked: Plasmoid.configuration.pin
                onToggled: Plasmoid.configuration.pin = checked
                glyph: "pin"
                text: i18n("Keep open")
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
