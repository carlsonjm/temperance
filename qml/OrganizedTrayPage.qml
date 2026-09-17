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
            x: 16
            y: Math.max(page.standardSpacing,
                scrollView.availableHeight - implicitHeight - page.surfaceSpacing)
            width: scrollView.availableWidth - 32
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
                    spacing: page.standardSpacing

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

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: Kirigami.Units.smallSpacing
                        Layout.rightMargin: Kirigami.Units.smallSpacing
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Heading {
                            Layout.fillWidth: true
                            text: section.modelData.title
                            level: 3
                        }
                    }

                    Item {
                        Layout.fillWidth: true
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
                                    anchors.leftMargin: 4
                                    anchors.rightMargin: 4
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
