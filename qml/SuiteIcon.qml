/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
import QtQuick

Item {
    required property string glyph
    implicitWidth: 20
    implicitHeight: 20

    Image {
        anchors.fill: parent
        source: Qt.resolvedUrl("../assets/icons/lucide/" + parent.glyph + ".svg")
        fillMode: Image.PreserveAspectFit
        smooth: true
    }
}
