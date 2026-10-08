/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
import QtQuick
import org.kde.kirigami as Kirigami

Item {
    id: suiteIcon
    required property string glyph
    property real glyphInset: 0
    // The glyphs are drawn in Ghost White. On a light ground they are recoloured
    // to the theme's text; on a dark one the drawing is shown as it is.
    readonly property color ink: tone.text
    readonly property url source: "qrc:/qt/qml/plasma/applet/studio/warbler/temperance/"
        + glyph + ".svg"
    objectName: "suiteIcon-" + glyph
    implicitWidth: 20
    implicitHeight: 20

    StatusColors {
        id: tone
        theme: suiteIcon.Kirigami.Theme
    }

    Image {
        objectName: tone.dark ? "suiteIconImage" : ""
        visible: tone.dark
        anchors.fill: parent
        anchors.margins: suiteIcon.glyphInset
        source: tone.dark ? suiteIcon.source : ""
        fillMode: Image.PreserveAspectFit
        smooth: true
    }

    Kirigami.Icon {
        objectName: tone.dark ? "" : "suiteIconImage"
        visible: !tone.dark
        anchors.fill: parent
        anchors.margins: suiteIcon.glyphInset
        source: tone.dark ? "" : suiteIcon.source
        isMask: true
        color: suiteIcon.ink
    }
}
