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

Item {
    id: page

    required property var notificationModel
    required property var clearHistory
    required property var resolveApplicationIcon
    required property var launchApplication
    required property bool demoNotificationVisible
    required property real maximumHeight

    implicitHeight: Math.min(maximumHeight, notificationContent.implicitHeight + 24)

    component NotificationActionPill: PlasmaComponents.ToolButton {
        id: pill
        display: PlasmaComponents.AbstractButton.TextOnly
        implicitHeight: 44
        leftPadding: 10
        rightPadding: 10
        Accessible.name: text
        // ToolButton handles Space; consume Enter here before the card's
        // default/open key handler can receive it through parent propagation.
        Keys.onReturnPressed: clicked()
        Keys.onEnterPressed: clicked()
        contentItem: PlasmaComponents.Label {
            text: pill.text
            textFormat: Text.PlainText
            color: "#F8F8FF"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
        }
        background: Rectangle {
            objectName: "notificationActionBackground"
            anchors.centerIn: parent
            width: Math.max(32, parent.width - 8)
            height: 30
            radius: height / 2
            color: pill.down ? Qt.rgba(1, 1, 1, 0.24)
                : pill.hovered ? Qt.rgba(1, 1, 1, 0.12)
                : Qt.rgba(1, 1, 1, 0.07)
            border.width: pill.activeFocus ? 1 : 0
            border.color: "#F8F8FF"
        }
    }

    ColumnLayout {
        id: notificationContent
        anchors.fill: parent
        anchors.leftMargin: Kirigami.Units.largeSpacing
        anchors.rightMargin: Kirigami.Units.largeSpacing
        anchors.bottomMargin: 12
        spacing: Kirigami.Units.mediumSpacing

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            visible: page.notificationModel.count > 0 || page.demoNotificationVisible

            PlasmaComponents.Label {
                Layout.fillWidth: true
                text: page.notificationModel.unreadNotificationsCount > 0
                    ? i18np("%1 unread", "%1 unread", page.notificationModel.unreadNotificationsCount)
                    : i18n("Recent")
                opacity: 0.72
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
            radius: 18
            color: Qt.rgba(1, 1, 1, 0.075)

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
                        text: i18n("Temperance")
                        font.weight: Font.DemiBold
                    }
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: i18n("Demo notification")
                    }
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: i18n("The adaptive rail and notification history are working.")
                        opacity: 0.68
                        wrapMode: Text.Wrap
                    }
                }
            }
        }

        ListView {
            id: historyView
            displaced: Transition {
                NumberAnimation {
                    properties: "x,y"
                    duration: 220
                    easing.type: Easing.OutCubic
                }
            }
            add: Transition {
                NumberAnimation {
                    property: "opacity"
                    from: 0
                    to: 1
                    duration: 140
                    easing.type: Easing.OutCubic
                }
            }
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? contentHeight : 0
            Layout.minimumHeight: 0
            Layout.fillHeight: true
            visible: count > 0
            clip: true
            spacing: 0
            model: page.notificationModel

            delegate: Item {
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
                width: historyView.width
                height: isGroup ? 44 : notificationCard.height + 8

                RowLayout {
                    id: groupHeading
                    visible: historyItem.isGroup
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: Kirigami.Units.smallSpacing
                    anchors.rightMargin: Kirigami.Units.smallSpacing
                    spacing: Kirigami.Units.smallSpacing

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
                        icon.source: "qrc:/qt/qml/plasma/applet/studio/warbler/temperance/trash-2.svg"
                        icon.color: "#F8F8FF"
                        contentItem: SuiteIcon {
                            glyph: "trash-2"
                            implicitWidth: 20; implicitHeight: 20
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
                            ? "qrc:/qt/qml/plasma/applet/studio/warbler/temperance/chevron-up.svg"
                            : "qrc:/qt/qml/plasma/applet/studio/warbler/temperance/chevron-down.svg"
                        icon.color: "#F8F8FF"
                        contentItem: SuiteIcon {
                            glyph: historyItem.isGroupExpanded ? "chevron-up" : "chevron-down"
                            implicitWidth: 20; implicitHeight: 20
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

                Rectangle {
                    id: notificationCard
                    objectName: "notificationCard"
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
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: notificationCardContent.implicitHeight
                        + Kirigami.Units.mediumSpacing * 2
                    radius: 18
                    color: Qt.rgba(1, 1, 1, 0.075)

                    RowLayout {
                        id: notificationCardContent
                        objectName: "notificationCardContent"
                        anchors.fill: parent
                        anchors.leftMargin: Kirigami.Units.largeSpacing
                        anchors.rightMargin: Kirigami.Units.largeSpacing
                        anchors.topMargin: Kirigami.Units.mediumSpacing
                        anchors.bottomMargin: Kirigami.Units.mediumSpacing
                        spacing: Kirigami.Units.mediumSpacing

                        Item {
                            // Group headings own the app identity, keeping child
                            // alerts aligned like pills beneath a Tray section.
                            Layout.preferredWidth: Kirigami.Units.iconSizes.small
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            RowLayout {
                                id: notificationHeader
                                objectName: "notificationHeader"
                                Layout.fillWidth: true
                                visible: applicationLabel.visible || summaryLabel.visible
                                spacing: 4

                                PlasmaComponents.Label {
                                    id: applicationLabel
                                    objectName: "notificationApplication"
                                    Layout.fillWidth: !summaryLabel.visible
                                    Layout.maximumWidth: summaryLabel.visible
                                        ? Kirigami.Units.gridUnit * 8 : Number.POSITIVE_INFINITY
                                    visible: !historyItem.isInGroup
                                        && historyItem.applicationName.length > 0
                                    text: historyItem.applicationName
                                    opacity: 0.68
                                    elide: Text.ElideRight
                                }

                                PlasmaComponents.Label {
                                    objectName: "notificationHeaderSeparator"
                                    visible: applicationLabel.visible && summaryLabel.visible
                                    text: "·"
                                    opacity: 0.42
                                }

                                PlasmaComponents.Label {
                                    id: summaryLabel
                                    objectName: "notificationSummary"
                                    Layout.fillWidth: true
                                    visible: text.length > 0
                                    text: historyItem.summary
                                    textFormat: Text.PlainText
                                    font.weight: Font.DemiBold
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
                                opacity: 0.68
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
                                        ? Qt.rgba(1, 1, 1, 0.12)
                                        : Qt.rgba(1, 1, 1, 0.07)
                                    border.width: detailsButton.activeFocus ? 1 : 0
                                    border.color: "#F8F8FF"
                                    Behavior on color { ColorAnimation { duration: 120 } }
                                }
                            }

                            Flow {
                                id: actionFlow
                                Layout.fillWidth: true
                                Layout.topMargin: 4
                                spacing: 6
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
                                    onClicked: page.notificationModel.close(
                                        page.notificationModel.index(historyItem.index, 0))
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
            visible: page.notificationModel.count === 0 && !page.demoNotificationVisible

            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width
                spacing: Kirigami.Units.mediumSpacing

                BellGlyph {
                    objectName: "notificationEmptyBell"
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Kirigami.Units.iconSizes.huge
                    Layout.preferredHeight: Kirigami.Units.iconSizes.huge
                    glyphColor: "#F8F8FF"
                    strokeWidth: 1.4
                    opacity: 0.34
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: i18n("All caught up")
                    horizontalAlignment: Text.AlignHCenter
                    font.weight: Font.DemiBold
                    font.pixelSize: Kirigami.Theme.defaultFont.pixelSize * 1.2
                }
            }
        }
    }
}
