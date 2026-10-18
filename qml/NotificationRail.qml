/*
    SPDX-FileCopyrightText: 2011 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2020 Konrad Materka <materka@gmail.com>
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import org.kde.draganddrop as DnD
import org.kde.kirigami as Kirigami
import org.kde.kitemmodels as KItemModels
import org.kde.notificationmanager as NotificationManager
import org.kde.plasma.clock as PlasmaClock
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import org.kde.plasma.workspace.dbus as DBus

Rectangle {
    id: notificationRail
    objectName: "temperance-notification-rail"
    // The Status Bar whose state this draws, and what of it this reads.
    required property Item bar
    required property StatusColors tone
    required property Timer notificationBatchTimer
    required property var railNotifications
    required property NumberAnimation tickerAway
    readonly property alias eventExit: eventExit
    readonly property alias eventFade: eventFade
    readonly property alias eventLayer: eventLayer
    readonly property alias liveNotificationView: liveNotificationView
    readonly property alias notificationContentArea: notificationContentArea
    readonly property alias notificationControls: notificationControls
    readonly property alias notificationHide: notificationHide
    readonly property alias notificationIntro: notificationIntro
    readonly property alias notificationMessageLayer: notificationMessageLayer
    readonly property alias notificationReveal: notificationReveal
    readonly property alias reviewNotificationsButton: reviewNotificationsButton
    readonly property alias tickerDrag: tickerDrag

    component RailTicker: Item {
        id: ticker
        required property string text
        // Styled text carries its own colours; what a reader hears is plain.
        property bool styled: false
        property string accessibleText: text
        property bool scrollingEnabled: true
        property bool alignRight: false
        property bool exposeOverflow: false
        implicitWidth: tickerLabel.implicitWidth
        implicitHeight: tickerLabel.implicitHeight
        Layout.preferredWidth: implicitWidth
        Layout.minimumWidth: 0
        clip: !exposeOverflow

        readonly property real overflow: Math.max(0, tickerLabel.implicitWidth - width)
        readonly property real contentWidth: tickerLabel.implicitWidth
        readonly property real restingX: alignRight ? Math.max(0, width - tickerLabel.implicitWidth) : 0

        function syncScroll() {
            scrollDelay.stop();
            tickerScroll.stop();
            tickerLabel.x = restingX;
            if (bar.motionEnabled && scrollingEnabled && overflow > 2) scrollDelay.restart();
        }

        PlasmaComponents.Label {
            id: tickerLabel
            objectName: "temperance-ticker-label"
            x: 0
            anchors.verticalCenter: parent.verticalCenter
            text: ticker.text
            textFormat: ticker.styled ? Text.StyledText : Text.PlainText
            maximumLineCount: 1
            Accessible.name: ticker.accessibleText
            // A line too long for the ticker ends in an ellipsis while it
            // stands still, and is whole while it flies in or scrolls.
            width: !ticker.exposeOverflow && !tickerScroll.running
                ? Math.min(implicitWidth, ticker.width) : implicitWidth
            elide: Text.ElideRight
            Behavior on opacity {
                enabled: bar.motionEnabled
                NumberAnimation { duration: Kirigami.Units.shortDuration; easing.type: Easing.OutCubic }
            }
            // Elsewhere the label keeps the system font it starts with.
            Binding { target: tickerLabel; property: "font"; value: bar.slimFont; when: bar.slim }
        }

        onTextChanged: {
            tickerLabel.x = restingX;
            tickerLabel.opacity = 0;
            tickerReveal.restart();
            Qt.callLater(ticker.syncScroll);
        }
        onScrollingEnabledChanged: syncScroll()
        onOverflowChanged: resizeSettle.restart()
        SequentialAnimation {
            id: tickerReveal
            PauseAnimation { duration: bar.motionEnabled ? 30 : 0 }
            NumberAnimation {
                target: tickerLabel
                property: "opacity"
                to: 1
                duration: bar.motionEnabled ? Kirigami.Units.longDuration : 0
                easing.type: Easing.OutCubic
            }
        }

        Timer {
            id: scrollDelay
            interval: 1800
            repeat: false
            onTriggered: {
                if (bar.motionEnabled && ticker.scrollingEnabled && ticker.overflow > 2)
                    tickerScroll.restart();
            }
        }

        Timer {
            id: resizeSettle
            interval: 230
            repeat: false
            onTriggered: ticker.syncScroll()
        }

        SequentialAnimation {
            id: tickerScroll
            NumberAnimation {
                target: tickerLabel
                property: "x"
                from: ticker.restingX
                to: -ticker.overflow
                duration: Math.max(1450, ticker.overflow * 20)
                easing.type: Easing.Linear
            }
            PauseAnimation { duration: 1400 }
            NumberAnimation {
                target: tickerLabel
                property: "x"
                from: -ticker.overflow
                to: ticker.restingX
                duration: 420
                easing.type: Easing.OutCubic
            }
            // One deliberate read per reveal. Leaving and returning to the
            // control is the explicit gesture that makes it readable again.
        }
    }

    component RailControlButton: PlasmaComponents.ToolButton {
        id: railControlButton
        property color glyphColor: railControlButton.pressed ? Qt.lighter(bar.accentColor, 1.18)
            : railControlButton.hovered ? bar.accentColor : tone.text
        implicitWidth: bar.statusPitch
        implicitHeight: Math.min(32, bar.rowHeight)
        display: PlasmaComponents.AbstractButton.IconOnly
        Behavior on glyphColor { ColorAnimation { duration: 120 } }
        background: null
        function activate() { clicked(); }
        contentItem: StatusFace {
            touched: railControlButton.pressed || bar.reachedControl === railControlButton
            BellGlyph {
                objectName: "temperance-bell-glyph"
                width: bar.bellSize
                height: bar.bellSize
                anchors.centerIn: parent
                // Keep the same optical baseline when the clapper returns.
                anchors.verticalCenterOffset: bar.bellDrop
                strokeWidth: bar.bellStroke
                glyphColor: railControlButton.glyphColor
                clapperProgress: bar.bellMarked && !bar.slim ? 0 : 1
                slashed: bar.slim && bar.doNotDisturb
                opacity: bar.bellShowsCount ? 0 : 1
                visible: opacity > 0
                Behavior on opacity {
                    NumberAnimation { duration: Kirigami.Units.shortDuration; easing.type: Easing.InOutQuad }
                }
            }
            // The slim row's count, in the bell's place: a chip one 16 px
            // circle for a digit, longer for more.
            Rectangle {
                id: unreadChip
                objectName: "temperance-unread-chip"
                anchors.centerIn: parent
                height: 16
                width: Math.max(height, unreadLabel.implicitWidth + 8)
                radius: height / 2
                color: Kirigami.Theme.highlightColor
                opacity: bar.bellShowsCount ? 1 : 0
                visible: opacity > 0
                Behavior on opacity {
                    NumberAnimation { duration: Kirigami.Units.shortDuration; easing.type: Easing.InOutQuad }
                }
                PlasmaComponents.Label {
                    id: unreadLabel
                    anchors.centerIn: parent
                    text: bar.unreadText
                    textFormat: Text.PlainText
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    font.features: { "tnum": 1 }
                    color: Kirigami.Theme.highlightedTextColor
                }
            }
        }
    }

    Layout.fillWidth: true
    // With notifications off the rail holds nothing, so it asks
    // for no room: the width above leaves none for it, and a
    // floor here would push the battery off the row's end.
    Layout.minimumWidth: bar.notificationsEnabled ? bar.statusPitch + 8 : 0
    Layout.fillHeight: true
    radius: height / 2
    color: "transparent"
    RowLayout {
        anchors.fill: parent
        spacing: 4
        layoutDirection: Qt.RightToLeft

        Rectangle {
            id: notificationControls
            objectName: "temperance-ticker-controls"
            visible: bar.notificationsEnabled
            property bool layoutOpen: active || revealed
            // The notification control is a permanent live icon beside the
            // status group, one status pitch wide; the ticker has the rest.
            Layout.preferredWidth: bar.statusPitch
            Layout.minimumWidth: Layout.preferredWidth
            Layout.maximumWidth: Layout.preferredWidth
            Layout.fillHeight: true
            property bool revealed: false
            readonly property bool active: bar.systemTrayState.activeApplet === null
                && bar.systemTrayState.page === "notifications"
                && bar.systemTrayState.expanded
            color: "transparent"
            clip: true
            // A resting pointer, a pen held over it included, shows the
            // latest line. A finger does not rest: on the bell it
            // opens the history. Its press also arrives as a hover
            // from the touchscreen, which is not a pointer resting.
            HoverHandler {
                id: sharedControlsHover
                onHoveredChanged: notificationControls.syncSharedHover()
            }
            function pointerResting() {
                return sharedControlsHover.hovered
                    && Date.now() - bar.lastTouchAt > 600
                    && !(sharedControlsHover.point.device
                        && sharedControlsHover.point.device.type === PointerDevice.TouchScreen);
            }
            function syncSharedHover() {
                const pointerInside = pointerResting();
                if (pointerInside) {
                    sharedHoverRelease.stop();
                    revealed = true;
                } else {
                    sharedHoverRelease.restart();
                }
            }
            onRevealedChanged: {
                if (revealed) {
                    controlsCollapseDelay.stop();
                    layoutOpen = true;
                    if (!bar.notificationReviewComplete) bar.revealNotificationText();
                } else {
                    bar.hideNotificationText();
                    bar.eventPage = false;
                    if (!active) controlsCollapseDelay.restart();
                }
            }
            onActiveChanged: {
                if (active) {
                    controlsCollapseDelay.stop();
                    layoutOpen = true;
                } else if (!revealed) {
                    controlsCollapseDelay.restart();
                }
            }
            Timer {
                id: sharedHoverRelease
                interval: 280
                repeat: false
                onTriggered: {
                    const pointerInside = notificationControls.pointerResting();
                    if (!pointerInside) notificationControls.revealed = false;
                }
            }
            Timer {
                id: controlsCollapseDelay
                interval: 130
                repeat: false
                onTriggered: {
                    if (!notificationControls.revealed && !notificationControls.active)
                        notificationControls.layoutOpen = false;
                }
            }
            RailControlButton {
                id: reviewNotificationsButton
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                objectName: "temperance-review-notifications"
                z: 30
                // The slim row's bell, and a bell set to show no
                // count, says the count to a screen reader.
                text: !bar.slim && bar.bellCounts ? i18n("Review all notifications")
                    : bar.unreadCount > 0
                    ? i18ncp("@info:tooltip", "Notifications, %1 unread", "Notifications, %1 unread", bar.unreadCount)
                    : i18nc("@info:tooltip", "Notifications")
                onClicked: bar.openNotifications()
                // No tooltip in the slim row: the page it opens
                // names itself, and a name over the bar covers
                // the controls beside it.
            }

            Item {
                id: notificationCountBadge
                anchors.horizontalCenter: reviewNotificationsButton.horizontalCenter
                // Bell rim: centered canvas, its optical offset,
                // 0.708 normalized rim, half the stroke. Keep five
                // logical pixels below that painted rim, so the
                // count reads apart from the bell, and never past
                // the bottom of the control.
                y: Math.min(notificationControls.height - height,
                    notificationControls.height / 2 - bar.bellSize / 2
                    + bar.bellDrop + bar.bellSize * 0.708
                    + bar.bellStroke / 2 + 5)
                visible: opacity > 0
                // The slim row shows its count in the bell's place.
                opacity: bar.bellMarked && !bar.slim ? 1 : 0
                scale: bar.bellMarked ? 1 : 0.72
                width: Math.max(18, notificationCountLabel.implicitWidth + 6)
                height: countInk.tightBoundingRect.height
                z: 40
                Behavior on opacity {
                    NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
                }
                Behavior on scale {
                    NumberAnimation { duration: bar.motionEnabled ? 180 : 0; easing.type: Easing.OutBack }
                }

                TextMetrics {
                    id: countInk
                    font: notificationCountLabel.font
                    text: notificationCountLabel.text
                }
                PlasmaComponents.Label {
                    id: notificationCountLabel
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: -baselineOffset - countInk.tightBoundingRect.y
                    objectName: "temperance-notification-count"
                    text: bar.attentionPages > 0 ? bar.attentionPages : "✓"
                    // Larger than the clock's date, so the count
                    // reads at arm's length under the smaller bell.
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: tone.text
                }
            }
        }

        Item {
            id: notificationContentArea
            objectName: "temperance-ticker-content"
            visible: bar.tickerEnabled
            Layout.fillWidth: true
            Layout.fillHeight: true
            // The slim row gives the ticker its whole height, for
            // the finger that flicks it.
            Layout.topMargin: bar.slim ? 0 : 4
            Layout.bottomMargin: bar.slim ? 0 : 4
            clip: true

            Item {
                id: notificationMessageLayer
                width: parent.width
                height: parent.height
                x: 2
                opacity: 0
                readonly property real flybyWidth: railNotifications.count > 0 && liveNotificationView.currentItem
                    ? liveNotificationView.currentItem.flybyWidth
                    : demoNotificationTicker.contentWidth

            ListView {
                id: liveNotificationView
                anchors.fill: parent
                anchors.rightMargin: Kirigami.Units.smallSpacing
                visible: railNotifications.count > 0 && !bar.eventPage
                interactive: false
                // The outer content area owns the physical dock
                // boundary. This inner clip would cut off long text.
                clip: false
                orientation: ListView.Horizontal
                model: railNotifications
                delegate: RowLayout {
                    id: notificationDelegate
                    visible: ListView.isCurrentItem
                    required property string summary
                    required property string body
                    required property string applicationName
                    required property string applicationIconName
                    required property var model
                    // How long the application asked to be seen,
                    // in ms: 0 for never expires, -1 for the
                    // server's default.
                    readonly property int timeout: model.timeout !== undefined ? model.timeout : -1
                    width: liveNotificationView.width
                    height: liveNotificationView.height
                    readonly property real flybyWidth: notificationTicker.contentWidth
                    spacing: 0
                    RailTicker {
                        id: notificationTicker
                        objectName: "temperance-live-ticker"
                        Layout.fillWidth: true
                        alignRight: true
                        exposeOverflow: bar.notificationArriving
                        styled: true
                        text: bar.notificationTickerMarkup(notificationDelegate.applicationName,
                            notificationDelegate.summary, notificationDelegate.body)
                        accessibleText: bar.notificationDisplayText(notificationDelegate.applicationName,
                            notificationDelegate.summary, notificationDelegate.body)
                        scrollingEnabled: notificationControls.revealed
                            && !bar.notificationArriving && !notificationReveal.running
                            && !bar.notificationReviewComplete
                    }
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.rightMargin: Kirigami.Units.smallSpacing
                visible: railNotifications.count === 0 && bar.demoNotificationVisible
                    && !bar.eventPage
                spacing: 0
                RailTicker {
                    id: demoNotificationTicker
                    objectName: "temperance-demo-ticker"
                    Layout.fillWidth: true
                    alignRight: true
                    exposeOverflow: bar.notificationArriving
                    text: bar.demoNotificationTexts[bar.demoNotificationIndex]
                    scrollingEnabled: notificationControls.revealed
                        && !bar.notificationArriving && !notificationReveal.running
                        && !bar.notificationReviewComplete
                }
            }
            }

            // A new line comes out from beside the bell, as today's
            // event does, and rests against it rather than crossing
            // the gap to the dock. A line too long for the ticker
            // comes in until its start reaches the far edge and
            // stops there, cut off, as an event waits for review: it
            // scrolls only for a hover on the bell. Short or long, it
            // goes when the application's own popup time ends, so
            // today's event is not held behind it.
            SequentialAnimation {
                id: notificationIntro
                readonly property real arrival: bar.motionEnabled
                    ? Math.min(notificationMessageLayer.flybyWidth, notificationContentArea.width) : 0
                readonly property int arrivalDuration: bar.motionEnabled
                    ? Math.max(900, arrival * 20) : 0
                readonly property bool longLine:
                    notificationMessageLayer.flybyWidth > notificationContentArea.width + 2
                // The application's popup time; the server's
                // default where it gives none, and half a minute
                // for one that never expires.
                readonly property int popupTime: {
                    const item = liveNotificationView.currentItem;
                    const timeout = railNotifications.count > 0 && item ? item.timeout : -1;
                    return timeout > 0 ? timeout : timeout === 0 ? 30000 : 5000;
                }
                readonly property int restDuration:
                    Math.max(1500, popupTime - arrivalDuration)
                onRunningChanged: if (!running) bar.notificationResting = false
                ScriptAction {
                    script: {
                        bar.notificationCopyActive = true;
                        notificationMessageLayer.x = 2 + notificationIntro.arrival;
                        notificationMessageLayer.opacity = 0;
                    }
                }
                PauseAnimation { duration: bar.motionEnabled ? 210 : 0 }
                ScriptAction {
                    script: {
                        // Off the ticker's edge until it moves, so
                        // only a line that does not move fades in.
                        if (notificationIntro.arrival > 0)
                            notificationMessageLayer.opacity = 1;
                    }
                }
                ParallelAnimation {
                    NumberAnimation {
                        target: notificationMessageLayer
                        property: "opacity"
                        to: 1
                        duration: Kirigami.Units.longDuration
                        easing.type: Easing.OutCubic
                    }
                    NumberAnimation {
                        target: notificationMessageLayer
                        property: "x"
                        to: 2
                        duration: notificationIntro.arrivalDuration
                        easing.type: Easing.OutQuad
                    }
                }
                ScriptAction { script: bar.notificationResting = true }
                PauseAnimation { duration: notificationIntro.restDuration }
                NumberAnimation {
                    target: notificationMessageLayer
                    property: "opacity"
                    to: 0
                    duration: bar.motionEnabled ? Kirigami.Units.shortDuration : 0
                    easing.type: Easing.InCubic
                }
                ScriptAction {
                    script: {
                        bar.notificationResting = false;
                        notificationMessageLayer.opacity = 0;
                        notificationMessageLayer.x = 2;
                        // Keep weather suppressed between items while a
                        // collected batch is still reading itself out.
                        if (!bar.notificationSequenceActive) {
                            bar.notificationCopyActive = false;
                        }
                    }
                }
                onFinished: {
                    if (!bar.notificationSequenceActive
                            || notificationControls.revealed
                            || railNotifications.count === 0) {
                        bar.notificationSequenceActive = false;
                        bar.notificationCopyActive = false;
                        return;
                    }
                    liveNotificationView.currentIndex = 0;
                    liveNotificationView.positionViewAtIndex(0, ListView.Beginning);
                    bar.notificationSequenceActive = false;
                    bar.notificationCopyActive = false;
                    bar.notificationAutoBatchSize = 0;
                    notificationBatchTimer.restart();
                }
            }

            SequentialAnimation {
                id: notificationReveal
                ScriptAction {
                    script: {
                        bar.notificationCopyActive = true;
                        notificationMessageLayer.x = 2;
                        notificationMessageLayer.opacity = 0;
                    }
                }
                PauseAnimation { duration: bar.motionEnabled ? 60 : 0 }
                NumberAnimation {
                    target: notificationMessageLayer
                    property: "opacity"
                    to: 1
                    duration: bar.motionEnabled ? Kirigami.Units.longDuration : 0
                    easing.type: Easing.OutCubic
                }
            }

            SequentialAnimation {
                id: notificationHide
                NumberAnimation {
                    target: notificationMessageLayer
                    property: "opacity"
                    to: 0
                    duration: bar.motionEnabled ? Kirigami.Units.shortDuration : 0
                    easing.type: Easing.InCubic
                }
                ScriptAction {
                    script: {
                        notificationMessageLayer.x = 2;
                        bar.notificationCopyActive = false;
                    }
                }
            }

            TapHandler {
                enabled: bar.notificationCopyActive
                onTapped: bar.openNotifications()
            }

            DragHandler {
                id: tickerDrag
                target: null
                yAxis.enabled: false
                enabled: bar.hasAttention && !tickerAway.running
                onActiveChanged: active ? bar.beginTickerCarry() : bar.endTickerCarry()
                onActiveTranslationChanged: if (active) bar.carryTicker(activeTranslation.x)
            }

            // The calendar's next event. It fades in where a
            // notification rests, and holds until it is read, it
            // ends, or a notification needs the ticker.
            Item {
                id: eventLayer
                objectName: "temperance-ticker-event"
                anchors.fill: parent
                anchors.leftMargin: 2
                anchors.rightMargin: Kirigami.Units.smallSpacing
                opacity: 0
                visible: opacity > 0
                property real offset: 0
                readonly property string eventKey: bar.tickerEvent ? bar.tickerEvent.key : ""
                readonly property real restingX: Math.max(0, width - eventLine.width)

                Accessible.role: Accessible.StaticText
                Accessible.name: bar.tickerEvent
                    ? bar.eventTimeText(bar.tickerEvent) + ", " + bar.tickerEvent.title : ""

                // Faded in where it rests: a line sliding in
                // past the bell's edge shows a letter cut in half.
                function enter() {
                    eventExit.stop();
                    offset = 0;
                    eventFade.restart();
                }

                function leave() {
                    eventFade.stop();
                    eventExit.restart();
                }

                readonly property bool shown: bar.eventShown
                onShownChanged: shown ? enter() : leave()
                onEventKeyChanged: if (shown) enter()
                Component.onCompleted: if (shown) enter()

                RowLayout {
                    id: eventLine
                    x: eventLayer.restingX + eventLayer.offset
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(implicitWidth, eventLayer.width)
                    spacing: 6

                    PlasmaComponents.Label {
                        id: eventTimeLabel
                        readonly property string lead: bar.eventTimeText(bar.tickerEvent)
                        readonly property string period: bar.eventTimePeriod(lead)
                        Layout.alignment: Qt.AlignBaseline
                        text: period ? lead.slice(0, lead.length - period.length - 1) : lead
                        textFormat: Text.PlainText
                        color: tone.secondaryText
                        Binding { target: eventTimeLabel; property: "font"; value: bar.slimFont; when: bar.slim }
                    }
                    PlasmaComponents.Label {
                        id: eventPeriodLabel
                        visible: eventTimeLabel.period.length > 0
                        Layout.alignment: Qt.AlignBaseline
                        Layout.leftMargin: -eventLine.spacing / 2
                        text: eventTimeLabel.period
                        textFormat: Text.PlainText
                        font: bar.slim ? bar.slimSmallFont : Kirigami.Theme.smallFont
                        color: tone.secondaryText
                    }
                    PlasmaComponents.Label {
                        id: eventTitleLabel
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignBaseline
                        Layout.maximumWidth: Math.max(0, eventLayer.width
                            - eventTimeLabel.implicitWidth - eventDot.width - eventLine.spacing * 2
                            - (eventPeriodLabel.visible ? eventPeriodLabel.implicitWidth
                                + eventLine.spacing / 2 : 0))
                        text: bar.tickerEvent ? bar.tickerEvent.title : ""
                        textFormat: Text.PlainText
                        color: tone.text
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        Binding { target: eventTitleLabel; property: "font"; value: bar.slimFont; when: bar.slim }
                    }
                    // A notification passes; the dot says this one
                    // stays. It takes the calendar's colour where the
                    // feed gives one.
                    Rectangle {
                        id: eventDot
                        objectName: "temperance-ticker-event-dot"
                        Layout.preferredWidth: 6
                        Layout.preferredHeight: 6
                        Layout.alignment: Qt.AlignVCenter
                        radius: 3
                        color: bar.tickerEvent && bar.tickerEvent.color
                            ? bar.tickerEvent.color : tone.secondaryText
                    }
                }

                NumberAnimation {
                    id: eventFade
                    target: eventLayer
                    property: "opacity"
                    to: 1
                    duration: bar.motionEnabled ? Kirigami.Units.longDuration : 0
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    id: eventExit
                    target: eventLayer
                    property: "opacity"
                    to: 0
                    duration: bar.motionEnabled ? Kirigami.Units.shortDuration : 0
                    easing.type: Easing.InCubic
                }

                TapHandler {
                    enabled: eventLayer.opacity > 0
                    onTapped: bar.openNotifications()
                }
            }
        }
    }

}
