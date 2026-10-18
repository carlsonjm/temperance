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

PlasmaCore.Dialog {
    id: criticalAlertPopup
    // The Status Bar whose state this draws, and what of it this reads.
    required property Item bar
    required property StatusColors tone
    required property Item criticalPopupAnchor
    required property var notificationHistory
    required property var priorityNotifications

    component BannerActionButton: Item {
        id: bannerActionButton
        property bool closeGlyph: false
        property string text: ""
        signal clicked()
        readonly property bool hovered: bannerActionHover.hovered
        readonly property bool pressed: bannerActionTap.pressed
        implicitWidth: 44
        implicitHeight: 40
        Layout.preferredWidth: 44
        Layout.minimumWidth: 44
        Layout.maximumWidth: 44
        Layout.preferredHeight: 40
        Layout.minimumHeight: 40
        Layout.maximumHeight: 40
        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: text

        Rectangle {
            id: bannerActionVisual
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 28
            height: 28
            radius: height / 2
            color: bannerActionButton.hovered || bannerActionButton.pressed
                ? tone.wash(0.12) : "transparent"
            border.width: bannerActionButton.activeFocus ? 1 : 0
            border.color: tone.text
            Behavior on color { ColorAnimation { duration: 120 } }

            Canvas {
                id: bannerActionGlyph
                anchors.centerIn: parent
                width: 18
                height: 18
                antialiasing: true
                readonly property color ink: tone.text
                onInkChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);
                    ctx.beginPath();
                    ctx.strokeStyle = ink;
                    ctx.lineWidth = 2.2;
                    ctx.lineCap = "round";
                    if (bannerActionButton.closeGlyph) {
                        ctx.moveTo(5, 5);
                        ctx.lineTo(13, 13);
                        ctx.moveTo(13, 5);
                        ctx.lineTo(5, 13);
                    } else {
                        ctx.moveTo(5, 9);
                        ctx.lineTo(13, 9);
                    }
                    ctx.stroke();
                }
            }
        }

        HoverHandler {
            id: bannerActionHover
        }
        TapHandler {
            id: bannerActionTap
            onTapped: {
                bannerActionButton.forceActiveFocus();
                bannerActionButton.clicked();
            }
        }
        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Return
                    || event.key === Qt.Key_Enter
                    || event.key === Qt.Key_Space) {
                bannerActionButton.clicked();
                event.accepted = true;
            }
        }
        Accessible.onPressAction: bannerActionButton.clicked()
    }

    visualParent: criticalPopupAnchor
    location: Plasmoid.location
    type: PlasmaCore.Dialog.CriticalNotification
    floating: 10
    flags: Qt.WindowStaysOnTopHint
    hideOnWindowDeactivate: false
    backgroundHints: PlasmaCore.Dialog.NoBackground
    visible: priorityNotifications.count > 0 && bannerStack.stackHeight > 0

    mainItem: Item {
        id: bannerStack
        width: 360
        // Wayland rejects zero-size window geometry, even briefly
        // during a show/hide transition. Animate cards, not this host.
        height: Math.max(1, stackHeight)
        property real stackHeight: 0

        // The alert's own window takes the look's colours, whatever
        // ground the bar it comes from sits on.
        StatusColors {
            id: bannerTone
            theme: bannerStack.Kirigami.Theme
        }
        readonly property real heightLimit: bar.notificationPopupHeightLimit
        clip: true

        function reflow() {
            let used = 0;
            let full = false;
            for (let row = 0; row < bannerRepeater.count; ++row) {
                const card = bannerRepeater.itemAt(row);
                if (!card) continue;
                const gap = used > 0 ? 4 : 0;
                const cardHeight = Math.min(card.implicitHeight, heightLimit);
                if (full || used + gap + cardHeight > heightLimit) {
                    card.visible = false;
                    full = true;
                    continue;
                }
                card.y = used + gap;
                card.height = cardHeight;
                card.visible = true;
                used += gap + cardHeight;
            }
            stackHeight = used;
        }
        onHeightLimitChanged: Qt.callLater(reflow)

        Repeater {
            id: bannerRepeater
            model: priorityNotifications
            onItemAdded: Qt.callLater(bannerStack.reflow)
            onItemRemoved: Qt.callLater(bannerStack.reflow)

        delegate: Rectangle {
            id: criticalCard
            required property int index
            required property string summary
            required property string body
            required property string applicationName
            required property string applicationIconName
            required property bool hasDefaultAction
            readonly property bool hasApplicationIcon:
                applicationIconName.length > 0
            width: bannerStack.width
            // Keep the banner bounded by its message instead of by a
            // permanently wide action row. The 44 px action rail stays
            // comfortable to target while one uninterrupted text column
            // owns the remaining width beside it.
            implicitHeight: Math.max(68, criticalContent.implicitHeight + 24)
            visible: false
            clip: true
            property real entranceProgress: 0
            opacity: entranceProgress
            // In from the panel's side, as the pages come.
            transform: Translate {
                y: (1 - criticalCard.entranceProgress)
                    * (Plasmoid.location === PlasmaCore.Types.TopEdge ? -12 : 12)
            }
            onVisibleChanged: {
                if (visible) {
                    bannerEntrance.restart();
                } else {
                    bannerEntrance.stop();
                    entranceProgress = 0;
                }
            }
            NumberAnimation {
                id: bannerEntrance
                target: criticalCard
                property: "entranceProgress"
                from: 0
                to: 1
                duration: bar.motionEnabled ? 180 : 0
                easing.type: Easing.OutCubic
            }
            onImplicitHeightChanged: Qt.callLater(bannerStack.reflow)
            Behavior on y {
                enabled: criticalCard.visible
                NumberAnimation { duration: bar.motionEnabled ? 220 : 0; easing.type: Easing.OutCubic }
            }
            color: bannerTone.surface
            radius: 12
            border.width: 1
            border.color: bannerTone.line

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    const modelIndex = priorityNotifications.mapToSource(
                        priorityNotifications.index(criticalCard.index, 0));
                    if (criticalCard.hasDefaultAction) {
                        notificationHistory.invokeDefaultAction(modelIndex);
                    } else {
                        bar.minimizePriorityNotification(modelIndex);
                        bar.openSurface("notifications");
                    }
                }
            }

            RowLayout {
                id: criticalContent
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        PlasmaComponents.Label {
                            Layout.fillWidth: true
                            text: criticalCard.applicationName || i18n("System alert")
                            opacity: 0.62
                            elide: Text.ElideRight
                        }
                        PlasmaComponents.Label {
                            id: criticalSummary
                            Layout.fillWidth: true
                            text: criticalCard.summary
                            textFormat: Text.PlainText
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                            maximumLineCount: 4
                            elide: Text.ElideRight
                        }
                    }

                    PlasmaComponents.Label {
                        id: criticalBody
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: criticalCard.body
                        textFormat: Text.StyledText
                        opacity: 0.78
                        wrapMode: Text.Wrap
                        maximumLineCount: 8
                        elide: Text.ElideRight
                    }
                    PlasmaComponents.ToolButton {
                        text: i18n("Show more")
                        visible: criticalSummary.truncated || criticalBody.truncated
                        Layout.minimumHeight: 44
                        onClicked: {
                            bar.minimizePriorityNotification(priorityNotifications.mapToSource(
                                priorityNotifications.index(criticalCard.index, 0)));
                            bar.openNotifications();
                        }
                    }
                }

                Item {
                    id: actionRail
                    Layout.preferredWidth: 44
                    Layout.minimumWidth: 44
                    Layout.maximumWidth: 44
                    Layout.fillHeight: true
                    Layout.minimumHeight: actionButtons.implicitHeight
                        + (criticalCard.hasApplicationIcon
                            ? 8 + Kirigami.Units.iconSizes.small : 0)

                    ColumnLayout {
                        id: actionButtons
                        anchors.top: parent.top
                        anchors.right: parent.right
                        spacing: 0

                        BannerActionButton {
                            closeGlyph: true
                            text: i18n("Dismiss")
                            onClicked: notificationHistory.close(
                                priorityNotifications.mapToSource(
                                    priorityNotifications.index(criticalCard.index, 0)))
                        }

                        BannerActionButton {
                            text: i18n("Keep for review")
                            onClicked: bar.minimizePriorityNotification(
                                priorityNotifications.mapToSource(
                                    priorityNotifications.index(criticalCard.index, 0)))
                        }
                    }

                    Kirigami.Icon {
                        visible: criticalCard.hasApplicationIcon
                        anchors.right: parent.right
                        anchors.rightMargin: 6
                        anchors.bottom: parent.bottom
                        width: Kirigami.Units.iconSizes.small
                        height: width
                        source: criticalCard.applicationIconName
                    }
                }
            }

        }
        }
    }
}
