/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kitemmodels as KItemModels
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.core as PlasmaCore

Item {
    id: page
    readonly property int compactSpacing: 8
    readonly property int standardSpacing: 12
    readonly property int surfaceSpacing: 24
    // The popup's margin line, 12 px into the page. Tiles keep 4 px of their
    // cell on each side, so the grid starts 4 px outside the line and the
    // outer tiles' edges land on it, 8 px apart from one another.
    readonly property int marginInset: 12
    readonly property int tileInset: 4

    implicitWidth: Kirigami.Units.gridUnit * 25
    // The popup heading already contributes half of the intended header gap.
    // Keep 12 px here and 24 px below so the visible rhythm is 24 px at both ends.
    implicitHeight: trayContent.implicitHeight + standardSpacing + surfaceSpacing

    readonly property var sections: [
        {
            categories: ["ApplicationStatus"],
            title: i18n("Apps"),
            icon: "applications-other-symbolic"
        },
        {
            categories: ["Hardware", "Communications"],
            title: i18n("Devices"),
            icon: "computer-symbolic"
        },
        {
            categories: ["SystemServices", "UnknownCategory"],
            title: i18n("System"),
            icon: "preferences-system-symbolic"
        }
    ]

    PlasmaComponents.ScrollView {
        id: scrollView
        anchors.fill: parent
        background: null

        ColumnLayout {
            id: trayContent
            x: page.marginInset
            y: Math.max(page.standardSpacing,
                scrollView.availableHeight - implicitHeight - page.surfaceSpacing)
            width: scrollView.availableWidth - page.marginInset * 2
            // Layouts ignore invisible children, so this creates hierarchy
            // only between populated sections and fully collapses empty ones.
            spacing: page.surfaceSpacing

            Repeater {
                model: page.sections

                delegate: ColumnLayout {
                    id: section
                    required property int index
                    required property var modelData
                    Layout.fillWidth: true
                    visible: categoryModel.count > 0
                    // A label sits nearer its tiles than one section sits
                    // to the next.
                    spacing: page.compactSpacing

                    KItemModels.KSortFilterProxyModel {
                        id: categoryModel
                        filterRoleName: "category"
                        filterRowCallback: (sourceRow, sourceParent) => {
                            const idx = sourceModel.index(sourceRow, 0, sourceParent);
                            const category = sourceModel.data(idx, filterRole) || "UnknownCategory";
                            return section.modelData.categories.indexOf(category) !== -1;
                        }
                        Component.onCompleted: sourceModel = root.organizedTrayModel
                    }

                    // A quiet label rather than a heading: the sections are
                    // told apart by the space between them.
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: section.modelData.title
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        color: "#A8FFFFFF"
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.leftMargin: -page.tileInset
                        Layout.rightMargin: -page.tileInset
                        Layout.preferredHeight: categoryGrid.implicitHeight

                        GridView {
                            id: categoryGrid
                            anchors.fill: parent
                            readonly property int columns: 2
                            readonly property int rows: Math.ceil(count / columns)
                            readonly property int tileHeight: Kirigami.Units.gridUnit * 2.4
                            readonly property int rowGap: page.compactSpacing
                            implicitHeight: rows * tileHeight + Math.max(0, rows - 1) * rowGap
                            cellWidth: Math.floor(width / columns)
                            cellHeight: tileHeight + rowGap
                            interactive: false
                            clip: true
                            model: categoryModel

                            delegate: Item {
                                required property int index
                                required property int effectiveStatus
                                required property var model
                                width: categoryGrid.cellWidth
                                height: categoryGrid.cellHeight
                                ItemLoader {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: page.tileInset
                                    anchors.rightMargin: page.tileInset
                                    height: categoryGrid.tileHeight
                                    index: parent.index
                                    effectiveStatus: parent.effectiveStatus
                                    model: parent.model
                                    presentationStatus: PlasmaCore.Types.PassiveStatus
                                    cardBackground: true
                                    inlinePresentation: true
                                }
                            }
                        }
                    }

                }
            }
        }
    }
}
