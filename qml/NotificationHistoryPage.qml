/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.notificationmanager as NotificationManager
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.plasmoid

Item {
    id: page

    StatusColors {
        id: tone
        theme: page.Kirigami.Theme
    }

    required property var notificationModel
    required property var clearHistory
    required property var resolveApplicationIcon
    required property var launchApplication
    required property bool demoNotificationVisible
    required property real maximumHeight
    // Today's calendar events, which head the history, and how one is
    // dismissed and its times written.
    property var events: []
    property var dismissEvent: key => {}
    property var formatTime: value => value.toLocaleTimeString(Qt.locale(), Locale.ShortFormat)
    // Whether Temperance holds the notification service, and else the
    // program that does, as "vendor name", or empty when it is not known.
    property bool serviceAvailable: true
    property string serviceOwner: ""
    readonly property int compactSpacing: 8
    readonly property int standardSpacing: 12
    readonly property int surfaceSpacing: 24
    // The popup's margin line, 12 px into the page; a card's text sits 12 px
    // inside the card and 8 px from its top and bottom.
    readonly property int marginInset: 12
    readonly property int cardPaddingH: 12
    readonly property int cardPaddingV: 8

    // What came in during the last Do Not Disturb: { count, from, to }, or
    // null. Those notifications lead the history under "While you were
    // focused", each marked, and the rest follow under "Earlier".
    property var focusArrivals: null
    property var cameDuringFocus: (model, modelIndex) => false
    function arrival(row) {
        const model = page.notificationModel;
        return !!page.focusArrivals && !!model && page.cameDuringFocus(model, model.index(row, 0));
    }
    // The row "Earlier" stands above: the first one after the arrivals that
    // is not one of them, or -1.
    readonly property int firstEarlierRow: {
        const model = page.notificationModel;
        page.modelRevision;
        if (!page.focusArrivals || !model) return -1;
        const rows = historyView.count;
        let seenArrival = false;
        for (let row = 0; row < rows; ++row) {
            const isGroup = Boolean(model.data(model.index(row, 0), NotificationManager.Notifications.IsGroupRole));
            if (isGroup) continue;
            if (page.arrival(row)) {
                seenArrival = true;
                continue;
            }
            if (!seenArrival) return -1;
            // A group's heading goes under "Earlier" with its first card.
            return row > 0 && Boolean(model.data(model.index(row - 1, 0), NotificationManager.Notifications.IsGroupRole))
                ? row - 1 : row;
        }
        return -1;
    }
    // Counts every change to the history, so what is read from the model
    // row by row, like where "Earlier" goes, is read again after a change
    // that keeps the number of rows.
    property int modelRevision: 0
    Connections {
        target: page.notificationModel
        function onDataChanged() { ++page.modelRevision; }
        function onLayoutChanged() { ++page.modelRevision; }
        function onModelReset() { ++page.modelRevision; }
        function onRowsInserted() { ++page.modelRevision; }
        function onRowsRemoved() { ++page.modelRevision; }
        function onRowsMoved() { ++page.modelRevision; }
    }

    // Back to the top, where what came in is.
    function showArrivals() {
        historyView.positionViewAtBeginning();
    }

    readonly property real naturalHeight: notificationContent.implicitHeight + standardSpacing + surfaceSpacing
    implicitHeight: Math.min(maximumHeight, naturalHeight)

    component NotificationActionPill: PlasmaComponents.ToolButton {
        id: pill
        readonly property color visualFill: actionBackground.color
        readonly property color visualOutline: actionBackground.border.color
        readonly property real visualOutlineWidth: actionBackground.border.width
        readonly property real visualOpacity: actionBackground.opacity
        display: PlasmaComponents.AbstractButton.TextOnly
        implicitHeight: 44
        leftPadding: 14
        rightPadding: 14
        Accessible.name: text
        // ToolButton handles Space; consume Enter here before the card's
        // default/open key handler can receive it through parent propagation.
        Keys.onReturnPressed: clicked()
        Keys.onEnterPressed: clicked()
        contentItem: PlasmaComponents.Label {
            text: pill.text
            textFormat: Text.PlainText
            color: tone.text
            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
        }
        background: Item {
            Rectangle {
                id: actionBackground
                objectName: "notificationActionBackground"
                anchors.centerIn: parent
                width: parent.width
                height: 30
                radius: height / 2
                color: pill.hovered || pill.down
                    ? tone.wash(pill.down ? 0.20 : 0.12) : "transparent"
                border.width: 1
                border.color: tone.text
                opacity: pill.activeFocus ? 1 : 0.62
                Behavior on color { ColorAnimation { duration: 120 } }
                Behavior on opacity { NumberAnimation { duration: 120 } }
            }
        }
    }

    ColumnLayout {
        id: notificationContent
        anchors.fill: parent
        anchors.leftMargin: page.marginInset
        anchors.rightMargin: page.marginInset
        anchors.topMargin: page.standardSpacing
        anchors.bottomMargin: page.surfaceSpacing
        spacing: page.compactSpacing

        Item { Layout.fillHeight: true }

        // Today's events, drawn as notification cards are: the calendar where
        // an app's name goes, the event as the summary, its times as the body,
        // and the same Dismiss.
        ColumnLayout {
            objectName: "notificationEvents"
            Layout.fillWidth: true
            visible: page.events.length > 0
            spacing: 8

            PlasmaComponents.Label {
                Layout.fillWidth: true
                text: i18nc("@title the day's calendar events", "Today")
                font.pixelSize: 13
                font.weight: Font.Medium
                color: tone.secondaryText
            }

            Repeater {
                model: page.events
                delegate: Rectangle {
                    id: eventCard
                    required property var modelData
                    objectName: "notificationEventCard"
                    Layout.fillWidth: true
                    implicitHeight: eventCardContent.implicitHeight + page.cardPaddingV * 2
                    radius: 10
                    color: tone.wash(0.07)
                    border.width: 0
                    Accessible.role: Accessible.StaticText
                    Accessible.name: [eventCard.modelData.title, eventTimes.text].join(", ")

                    RowLayout {
                        id: eventCardContent
                        anchors.fill: parent
                        anchors.leftMargin: page.cardPaddingH
                        anchors.rightMargin: page.cardPaddingH
                        anchors.topMargin: page.cardPaddingV
                        anchors.bottomMargin: page.cardPaddingV
                        spacing: 8

                        Item { Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                PlasmaComponents.Label {
                                    Layout.maximumWidth: Kirigami.Units.gridUnit * 8
                                    visible: text.length > 0
                                    text: eventCard.modelData.calendar || ""
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                    color: tone.secondaryText
                                    elide: Text.ElideRight
                                }
                                PlasmaComponents.Label {
                                    visible: (eventCard.modelData.calendar || "").length > 0
                                    text: "·"
                                    opacity: 0.42
                                }
                                PlasmaComponents.Label {
                                    objectName: "notificationEventTitle"
                                    Layout.fillWidth: true
                                    text: eventCard.modelData.title
                                    textFormat: Text.PlainText
                                    font.pixelSize: 15
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                }
                            }
                            PlasmaComponents.Label {
                                id: eventTimes
                                Layout.fillWidth: true
                                text: page.formatTime(eventCard.modelData.start)
                                    + " – " + page.formatTime(eventCard.modelData.end)
                                font.pixelSize: 13
                                color: tone.secondaryText
                            }
                            Item {
                                implicitHeight: 44
                                Layout.fillWidth: true
                                Layout.preferredHeight: implicitHeight
                                Layout.topMargin: 4
                                NotificationActionPill {
                                    objectName: "notificationEventDismiss"
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: i18n("Dismiss")
                                    Accessible.name: i18n("Dismiss event: %1", eventCard.modelData.title)
                                    onClicked: page.dismissEvent(eventCard.modelData.key)
                                }
                            }
                        }
                    }
                }
            }
        }

        // Notifications are a group of their own below the day's events.
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: page.events.length > 0 ? page.surfaceSpacing - page.compactSpacing : 0
            visible: page.notificationModel.count > 0 || page.demoNotificationVisible

            PlasmaComponents.Label {
                objectName: "notificationGroupLabel"
                Layout.fillWidth: !page.focusArrivals
                text: page.focusArrivals ? i18n("While you were focused")
                    : page.notificationModel.unreadNotificationsCount > 0
                    ? i18np("%1 unread", "%1 unread", page.notificationModel.unreadNotificationsCount)
                    : i18n("Recent")
                font.pixelSize: 13
                font.weight: Font.Medium
                color: page.focusArrivals ? tone.text : tone.secondaryText
            }
            // How many, and when: one time when they all came in the same
            // minute. It takes the room left beside the label and Clear, and
            // is cut short there rather than run under Clear.
            PlasmaComponents.Label {
                objectName: "notificationFocusSpan"
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                Layout.minimumWidth: 0
                visible: !!page.focusArrivals
                horizontalAlignment: Text.AlignRight
                text: {
                    if (!page.focusArrivals) return "";
                    const from = page.formatTime(page.focusArrivals.from);
                    const to = page.formatTime(page.focusArrivals.to);
                    return from === to
                        ? i18nc("@label how many came in during Do Not Disturb, and when", "%1 · %2",
                            page.focusArrivals.count, from)
                        : i18nc("@label how many came in during Do Not Disturb, and from when to when", "%1 · %2 to %3",
                            page.focusArrivals.count, from, to);
                }
                font.pixelSize: 13
                color: tone.secondaryText
                elide: Text.ElideRight
                maximumLineCount: 1
            }
            PlasmaComponents.ToolButton {
                text: i18n("Clear")
                display: PlasmaComponents.AbstractButton.TextOnly
                onClicked: page.clearHistory()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: demoContent.implicitHeight + Kirigami.Units.largeSpacing * 2
            visible: page.demoNotificationVisible
            radius: 10
            color: tone.wash(0.07)

            RowLayout {
                id: demoContent
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.mediumSpacing

                Kirigami.Icon {
                    Layout.alignment: Qt.AlignTop
                    source: "notification-active"
                    implicitWidth: Kirigami.Units.iconSizes.medium
                    implicitHeight: implicitWidth
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: Plasmoid.title
                        font.pixelSize: 15
                        font.weight: Font.Medium
                    }
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: i18n("Demo notification")
                    }
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: i18n("The adaptive rail and notification history are working.")
                        font.pixelSize: 13
                        color: tone.secondaryText
                        wrapMode: Text.Wrap
                    }
                }
            }
        }

        // The history is a plain column of cards: each one stands where the
        // one above it ends, measured afresh whenever any card's height
        // changes, so no card can be drawn over another. A list view instead
        // remembers where it put each card and only moves the cards it is
        // told have changed, which left cards over one another after a group
        // opened, a card was dismissed or a long card arrived. The history
        // holds one heading and one card per app until a group is opened, so
        // making every card up front costs little.
        Flickable {
            id: historyView
            objectName: "notificationHistoryView"
            readonly property alias count: historyRows.count
            function positionViewAtBeginning() {
                contentY = 0;
            }
            // A wheel notch scrolls 80 pixels at Plasma's usual three lines per notch.
            Kirigami.WheelHandler { target: historyView; verticalStepSize: Qt.styleHints.wheelScrollLines * 80 / 3 }
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? contentHeight : 0
            Layout.minimumHeight: 0
            Layout.fillHeight: true
            visible: count > 0
            clip: true
            contentWidth: width
            contentHeight: historyColumn.height
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: historyColumn
                objectName: "notificationHistoryColumn"
                width: historyView.width
                // Nothing here animates where a row stands. A dismissed card
                // folds its own height away instead, and the rows below follow
                // that height frame by frame.

                Repeater {
                    id: historyRows
                    model: page.notificationModel

                    delegate: Column {
                        id: historyItem
                        objectName: "notificationRow" + index
                        required property int index
                        required property bool isGroup
                        required property bool isInGroup
                        required property bool isGroupExpanded
                        required property int groupChildrenCount
                        required property string summary
                        required property string body
                        required property string applicationName
                        required property string applicationIconName
                        required property string desktopEntry
                        required property bool hasDefaultAction
                        required property var actionNames
                        required property var actionLabels
                        readonly property var producerActions: {
                            const actions = [];
                            if (isGroup) return actions;
                            const names = actionNames || [];
                            const labels = actionLabels || [];
                            for (let i = 0; i < Math.min(names.length, labels.length); ++i) {
                                // NotificationManager excludes the default action from
                                // these roles; keep it exclusively on the card even if
                                // a producer/model supplies it in the named-action list.
                                if (names[i] && names[i] !== "default" && labels[i])
                                    actions.push({name: names[i], label: labels[i]});
                            }
                            return actions;
                        }
                        property bool detailsExpanded: false
                        // What the header shows, from the notification itself. Read
                        // through a label's visible, it would follow the page's own
                        // visibility: closing the bell would hide the header, and it
                        // would stay hidden when the bell opens again.
                        readonly property bool showsApplication: !isInGroup && applicationName.length > 0
                        readonly property bool showsSummary: summary.length > 0
                        readonly property bool canOpen: !isGroup
                            && (hasDefaultAction || desktopEntry.length > 0)
                        function openNotification() {
                            if (!canOpen) return;
                            if (hasDefaultAction) {
                                page.notificationModel.invokeDefaultAction(
                                    page.notificationModel.index(index, 0));
                            } else {
                                page.launchApplication(desktopEntry);
                            }
                        }
                        // Came in during the last Do Not Disturb, and the first of
                        // the rest, under "Earlier".
                        readonly property bool arrived: !isGroup && page.arrival(index)
                        readonly property bool startsEarlier: index === page.firstEarlierRow
                        // A row is its parts stacked, each where the one above ends:
                        // "Earlier" when it starts the older ones, then the app's
                        // heading or the card, then the 8 px between cards. Its height
                        // is theirs added up, so nothing in it can reach the next row.
                        width: historyView.width
                        bottomPadding: isGroup ? 0 : 8
                        height: implicitHeight * (1 - folded)
                        opacity: 1 - folded
                        clip: folded > 0

                        // Dismiss folds the card away, then closes it. The
                        // notification is found again by its id when the fold
                        // ends, in case the history changed meanwhile.
                        property real folded: 0
                        property var leavingId
                        function dismiss() {
                            if (foldAway.running) return;
                            const model = page.notificationModel;
                            leavingId = model.data(model.index(index, 0), NotificationManager.Notifications.IdRole);
                            if (Kirigami.Units.longDuration > 0) foldAway.start();
                            else close();
                        }
                        function close() {
                            const model = page.notificationModel;
                            let row = index;
                            if (leavingId !== undefined
                                    && model.data(model.index(row, 0), NotificationManager.Notifications.IdRole) !== leavingId) {
                                row = -1;
                                for (let other = 0; other < model.count; ++other) {
                                    if (model.data(model.index(other, 0), NotificationManager.Notifications.IdRole) === leavingId) {
                                        row = other;
                                        break;
                                    }
                                }
                            }
                            if (row >= 0) model.close(model.index(row, 0));
                            // The row leaves the history as it closes, and its
                            // index with it. If it is still here, the close
                            // did not take, and the card unfolds again.
                            if (index >= 0) folded = 0;
                        }
                        NumberAnimation {
                            id: foldAway
                            target: historyItem
                            property: "folded"
                            to: 1
                            duration: 200
                            easing.type: Easing.OutCubic
                            onFinished: historyItem.close()
                        }

                        PlasmaComponents.Label {
                            id: earlierLabel
                            objectName: "notificationEarlierLabel"
                            visible: historyItem.startsEarlier
                            width: parent.width
                            topPadding: 8
                            bottomPadding: 12
                            text: i18nc("@title the notifications from before Do Not Disturb", "Earlier")
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            color: tone.secondaryText
                        }

                        Item {
                            objectName: "notificationGroupHeading"
                            visible: historyItem.isGroup
                            width: parent.width
                            height: 44

                            RowLayout {
                                id: groupHeading
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                // The icon stands where a card's padding ends, so the name
                                // after it lines up with the cards' text below.
                                anchors.leftMargin: page.cardPaddingH
                                anchors.rightMargin: page.cardPaddingH
                                spacing: 8

                                Kirigami.Icon {
                                    source: page.resolveApplicationIcon(
                                        historyItem.applicationName,
                                        historyItem.desktopEntry,
                                        historyItem.applicationIconName)
                                    implicitWidth: Kirigami.Units.iconSizes.smallMedium
                                    implicitHeight: implicitWidth
                                }

                                Kirigami.Heading {
                                    Layout.fillWidth: true
                                    text: historyItem.applicationName || i18n("Notifications")
                                    level: 3
                                    elide: Text.ElideRight
                                }

                                PlasmaComponents.Label {
                                    text: historyItem.groupChildrenCount
                                    opacity: 0.58
                                    horizontalAlignment: Text.AlignHCenter
                                    Layout.minimumWidth: 20
                                }

                                PlasmaComponents.ToolButton {
                                    id: clearGroupButton
                                    objectName: "notificationClearGroup"
                                    text: i18n("Clear")
                                    icon.source: "qrc:/qt/qml/plasma/applet/co/goodinput/temperance/trash-2.svg"
                                    icon.color: tone.text
                                    // The row's controls share one size: a 16 px glyph in a 30 px
                                    // round button, as tall as the Dismiss pill. The glyph keeps its
                                    // size inside whatever room the button gives its content.
                                    implicitWidth: 30
                                    implicitHeight: 30
                                    padding: 7
                                    contentItem: Item {
                                        implicitWidth: 16
                                        implicitHeight: 16
                                        SuiteIcon {
                                            anchors.centerIn: parent
                                            glyph: "trash-2"
                                            width: 16
                                            height: 16
                                        }
                                    }
                                    // A plain round wash on hover and press: Plasma's own
                                    // button frame breaks up at this small size.
                                    background: Rectangle {
                                        implicitWidth: 30
                                        implicitHeight: 30
                                        radius: height / 2
                                        color: clearGroupButton.down ? tone.wash(0.20) : clearGroupButton.hovered ? tone.wash(0.12) : "transparent"
                                        Behavior on color { ColorAnimation { duration: 120 } }
                                    }
                                    display: PlasmaComponents.AbstractButton.IconOnly
                                    onClicked: page.notificationModel.close(
                                        page.notificationModel.index(historyItem.index, 0))
                                    PlasmaComponents.ToolTip {
                                        text: i18n("Clear notifications from %1",
                                            historyItem.applicationName || i18n("this app"))
                                    }
                                }

                                PlasmaComponents.ToolButton {
                                    id: expandGroupButton
                                    objectName: "notificationExpandGroup"
                                    icon.source: historyItem.isGroupExpanded
                                        ? "qrc:/qt/qml/plasma/applet/co/goodinput/temperance/chevron-up.svg"
                                        : "qrc:/qt/qml/plasma/applet/co/goodinput/temperance/chevron-down.svg"
                                    icon.color: tone.text
                                    // The row's controls share one size: a 16 px glyph in a 30 px
                                    // round button, as tall as the Dismiss pill. The glyph keeps its
                                    // size inside whatever room the button gives its content.
                                    implicitWidth: 30
                                    implicitHeight: 30
                                    padding: 7
                                    contentItem: Item {
                                        implicitWidth: 16
                                        implicitHeight: 16
                                        SuiteIcon {
                                            anchors.centerIn: parent
                                            glyph: historyItem.isGroupExpanded ? "chevron-up" : "chevron-down"
                                            width: 16
                                            height: 16
                                        }
                                    }
                                    // A plain round wash on hover and press: Plasma's own
                                    // button frame breaks up at this small size.
                                    background: Rectangle {
                                        implicitWidth: 30
                                        implicitHeight: 30
                                        radius: height / 2
                                        color: expandGroupButton.down ? tone.wash(0.20) : expandGroupButton.hovered ? tone.wash(0.12) : "transparent"
                                        Behavior on color { ColorAnimation { duration: 120 } }
                                    }
                                    display: PlasmaComponents.AbstractButton.IconOnly
                                    text: historyItem.isGroupExpanded
                                        ? i18n("Collapse") : i18n("Expand")
                                    onClicked: page.notificationModel.setData(
                                        page.notificationModel.index(historyItem.index, 0),
                                        !historyItem.isGroupExpanded,
                                        NotificationManager.Notifications.IsGroupExpandedRole)
                                    PlasmaComponents.ToolTip { text: expandGroupButton.text }
                                }
                            }
                        }

                        Rectangle {
                            id: notificationCard
                            objectName: "notificationCard"
                            readonly property real visualOutlineWidth: border.width
                            readonly property color visualOutline: border.color
                            activeFocusOnTab: historyItem.canOpen
                            Accessible.role: Accessible.Button
                            Accessible.name: historyItem.summary.length > 0 ? historyItem.summary
                                : historyItem.applicationName.length > 0 ? historyItem.applicationName
                                : historyItem.body
                            Accessible.onPressAction: historyItem.openNotification()
                            Keys.onReturnPressed: historyItem.openNotification()
                            Keys.onSpacePressed: historyItem.openNotification()
                            MouseArea {
                                anchors.fill: parent
                                enabled: historyItem.canOpen
                                onClicked: historyItem.openNotification()
                            }
                            visible: !historyItem.isGroup
                            width: parent.width
                            height: notificationCardContent.implicitHeight
                                + page.cardPaddingV * 2
                            radius: 10
                            color: tone.wash(historyItem.arrived ? 0.12 : 0.07)
                            border.width: 0

                            // What came in during Do Not Disturb: a 3 px edge that
                            // follows the card's corners.
                            Item {
                                objectName: "notificationFocusMark"
                                visible: historyItem.arrived
                                width: 3
                                height: parent.height
                                clip: true
                                Rectangle {
                                    width: notificationCard.radius * 2
                                    height: parent.height
                                    radius: notificationCard.radius
                                    color: tone.focusMark
                                }
                            }

                            RowLayout {
                                id: notificationCardContent
                                objectName: "notificationCardContent"
                                // Laid out from the top only: the card takes its
                                // height from this, never the other way round.
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.leftMargin: page.cardPaddingH
                                anchors.rightMargin: page.cardPaddingH
                                anchors.topMargin: page.cardPaddingV
                                spacing: 8

                                Item {
                                    // Group headings own the app identity, keeping child
                                    // alerts aligned like pills beneath a Tray section.
                                    Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    RowLayout {
                                        id: notificationHeader
                                        objectName: "notificationHeader"
                                        Layout.fillWidth: true
                                        visible: historyItem.showsApplication || historyItem.showsSummary
                                        spacing: 4

                                        PlasmaComponents.Label {
                                            id: applicationLabel
                                            objectName: "notificationApplication"
                                            Layout.fillWidth: !historyItem.showsSummary
                                            Layout.maximumWidth: historyItem.showsSummary
                                                ? Kirigami.Units.gridUnit * 8 : Number.POSITIVE_INFINITY
                                            visible: historyItem.showsApplication
                                            text: historyItem.applicationName
                                            font.pixelSize: 13
                                            font.weight: Font.Medium
                                            color: tone.secondaryText
                                            elide: Text.ElideRight
                                        }

                                        PlasmaComponents.Label {
                                            objectName: "notificationHeaderSeparator"
                                            visible: historyItem.showsApplication && historyItem.showsSummary
                                            text: "·"
                                            opacity: 0.42
                                        }

                                        PlasmaComponents.Label {
                                            id: summaryLabel
                                            objectName: "notificationSummary"
                                            Layout.fillWidth: true
                                            visible: historyItem.showsSummary
                                            text: historyItem.summary
                                            textFormat: Text.PlainText
                                            font.pixelSize: 15
                                            font.weight: Font.Medium
                                            wrapMode: Text.Wrap
                                            maximumLineCount: historyItem.detailsExpanded ? 1000 : 1
                                            elide: historyItem.detailsExpanded
                                                ? Text.ElideNone : Text.ElideRight
                                        }
                                    }
                                    PlasmaComponents.Label {
                                        id: bodyLabel
                                        objectName: "notificationBody"
                                        Layout.fillWidth: true
                                        visible: text.length > 0 && text !== historyItem.summary
                                        text: historyItem.body
                                        // StyledText supports bounded lines/truncation;
                                        // AutoText may choose RichText, which does not.
                                        textFormat: Text.StyledText
                                        font.pixelSize: 13
                                        color: tone.secondaryText
                                        wrapMode: Text.Wrap
                                        maximumLineCount: historyItem.detailsExpanded ? 1000 : 2
                                        elide: historyItem.detailsExpanded
                                            ? Text.ElideNone : Text.ElideRight
                                    }

                                    PlasmaComponents.ToolButton {
                                        id: detailsButton
                                        objectName: "notificationReadMore"
                                        Layout.alignment: Qt.AlignRight
                                        Layout.minimumHeight: 44
                                        leftPadding: 12
                                        rightPadding: 12
                                        visible: historyItem.detailsExpanded
                                            || summaryLabel.truncated || bodyLabel.truncated
                                        text: historyItem.detailsExpanded
                                            ? i18n("Show less") : i18n("Show more")
                                        display: PlasmaComponents.AbstractButton.TextOnly
                                        onClicked: historyItem.detailsExpanded
                                            = !historyItem.detailsExpanded
                                        background: Rectangle {
                                            radius: height / 2
                                            color: detailsButton.hovered || detailsButton.down
                                                ? tone.wash(0.12)
                                                : tone.wash(0.07)
                                            border.width: detailsButton.activeFocus ? 1 : 0
                                            border.color: tone.text
                                            Behavior on color { ColorAnimation { duration: 120 } }
                                        }
                                    }

                                    Item {
                                        // At least the 44 px touch row; more when the
                                        // buttons wrap onto a second line.
                                        implicitHeight: Math.max(44, actionFlow.implicitHeight)
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: implicitHeight
                                        Layout.minimumHeight: implicitHeight
                                        Layout.topMargin: 4

                                        Flow {
                                            id: actionFlow
                                            objectName: "notificationActionFlow"
                                            width: parent.width
                                            anchors.verticalCenter: parent.verticalCenter
                                            spacing: 8
                                            // These controls stack above the card's earlier
                                            // MouseArea and accept their own pointer/key input.
                                            Repeater {
                                                model: historyItem.producerActions
                                                delegate: NotificationActionPill {
                                                    required property var modelData
                                                    objectName: "notificationAction-" + modelData.name
                                                    width: Math.min(implicitWidth, actionFlow.width)
                                                    text: modelData.label
                                                    onClicked: page.notificationModel.invokeAction(
                                                        page.notificationModel.index(historyItem.index, 0),
                                                        modelData.name)
                                                }
                                            }
                                            NotificationActionPill {
                                                objectName: "notificationDismiss"
                                                width: Math.min(implicitWidth, actionFlow.width)
                                                text: i18n("Dismiss")
                                                Accessible.name: i18n("Dismiss notification: %1", historyItem.summary)
                                                onClicked: historyItem.dismiss()
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 10
            visible: !page.serviceAvailable || (page.notificationModel.count === 0
                && !page.demoNotificationVisible && page.events.length === 0)

            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width
                spacing: Kirigami.Units.mediumSpacing

                BellGlyph {
                    objectName: "notificationEmptyBell"
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Kirigami.Units.iconSizes.huge
                    Layout.preferredHeight: Kirigami.Units.iconSizes.huge
                    glyphColor: tone.text
                    strokeWidth: 1.4
                    opacity: 0.34
                }
                PlasmaComponents.Label {
                    objectName: "notificationEmptyTitle"
                    Layout.fillWidth: true
                    text: page.serviceAvailable ? i18n("All caught up")
                        : i18n("Notifications aren't available")
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    font.weight: Font.DemiBold
                    font.pixelSize: Kirigami.Theme.defaultFont.pixelSize * 1.2
                }
                PlasmaComponents.Label {
                    objectName: "notificationEmptyDetail"
                    Layout.fillWidth: true
                    visible: !page.serviceAvailable
                    text: page.serviceOwner
                        ? i18nc("@info %1 is a program's vendor and name", "%1 is showing notifications instead.", page.serviceOwner)
                        : i18n("Another program is showing notifications instead.")
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    opacity: 0.65
                }
            }
        }
    }
}
