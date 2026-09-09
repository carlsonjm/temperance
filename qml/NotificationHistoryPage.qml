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
    required property bool demoNotificationVisible

    implicitHeight: Math.min(Kirigami.Units.gridUnit * 22, notificationContent.implicitHeight + 24)

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
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? Math.min(contentHeight, Kirigami.Units.gridUnit * 14) : 0
            visible: count > 0
            clip: true
            spacing: 0
            model: page.notificationModel

            delegate: Item {
                id: historyItem
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
                        text: i18n("Clear")
                        icon.name: "edit-clear-history"
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
                        icon.name: historyItem.isGroupExpanded
                            ? "go-up-symbolic" : "go-down-symbolic"
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
                    visible: !historyItem.isGroup
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: notificationCardContent.implicitHeight + Kirigami.Units.largeSpacing * 2
                    radius: 18
                    color: Qt.rgba(1, 1, 1, 0.075)

                    RowLayout {
                        id: notificationCardContent
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.largeSpacing
                        spacing: Kirigami.Units.mediumSpacing

                        Item {
                            // Group headings own the app identity, keeping child
                            // alerts aligned like pills beneath a Tray section.
                            Layout.preferredWidth: Kirigami.Units.iconSizes.small
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            PlasmaComponents.Label {
                                Layout.fillWidth: true
                                text: historyItem.summary
                                font.weight: Font.DemiBold
                                wrapMode: Text.Wrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                            }
                            PlasmaComponents.Label {
                                Layout.fillWidth: true
                                visible: text.length > 0 && text !== historyItem.summary
                                text: historyItem.body
                                opacity: 0.68
                                wrapMode: Text.Wrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
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

                Kirigami.Icon {
                    Layout.alignment: Qt.AlignHCenter
                    source: "notifications-symbolic"
                    implicitWidth: Kirigami.Units.iconSizes.huge
                    implicitHeight: implicitWidth
                    opacity: 0.34
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: i18n("All caught up")
                    horizontalAlignment: Text.AlignHCenter
                    font.weight: Font.DemiBold
                    font.pixelSize: Kirigami.Theme.defaultFont.pixelSize * 1.2
                }
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: i18n("New alerts will appear in the dock first.")
                    horizontalAlignment: Text.AlignHCenter
                    opacity: 0.62
                }
            }
        }
    }
}
