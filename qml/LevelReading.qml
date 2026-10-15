/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.kitemmodels as KItemModels
import org.kde.plasma.private.brightnesscontrolplugin as Brightness

// The volume or the screen's brightness, shown in the row for a moment as it
// changes, for a desktop that turns Plasma's own popups for them off
// (`showLevels`). The speaker or the sun, then how full: nothing more. It
// says nothing at start, nothing for a level that is only read again, and
// nothing while Control Center is open, whose sliders already say it.
Item {
    id: reading

    // The volume, as Status reads it: `percent`, `muted` and `available`.
    property QtObject audio: null
    // Whether to watch at all; off, nothing is read or shown.
    property bool watching: false
    // Whether Control Center is open, and the reading stays quiet.
    property bool quiet: false
    property color ink: "#F8F8FF"
    property color track: Qt.rgba(1, 1, 1, 0.24)
    property int pitch: 28
    property int iconSize: 16
    property bool motionEnabled: true

    // What it shows now: "volume", "brightness" or nothing.
    property string kind: ""
    readonly property bool showing: kind !== ""
    readonly property real level: kind === "volume"
        ? (audio && !audio.muted ? Math.max(0, Math.min(1, audio.percent / 100)) : 0)
        : Math.max(0, Math.min(1, brightnessMax > 0 ? brightness / brightnessMax : 0))

    implicitWidth: pitch + 34 + 8
    // The room it takes in the row, opening and closing with it so its
    // neighbours slide rather than jump.
    property real room: showing ? implicitWidth : 0
    Behavior on room {
        NumberAnimation { duration: reading.motionEnabled ? 160 : 0; easing.type: Easing.OutCubic }
    }
    opacity: showing ? 1 : 0
    visible: room > 0
    clip: true
    Behavior on opacity {
        NumberAnimation { duration: reading.motionEnabled ? 160 : 0; easing.type: Easing.OutCubic }
    }

    Accessible.role: Accessible.Indicator
    Accessible.name: kind === "volume" ? i18n("Volume %1%", Math.round(level * 100))
        : kind === "brightness" ? i18n("Brightness %1%", Math.round(level * 100)) : ""

    function show(what) {
        if (!watching || quiet) {
            return;
        }
        kind = what;
        hold.restart();
    }

    Timer {
        id: hold
        interval: 1500
        onTriggered: reading.kind = ""
    }

    // The volume: a change after the first reading is shown.
    property int lastPercent: -1
    property bool lastMuted: false
    Connections {
        target: reading.audio
        enabled: reading.watching
        function onVolumeChanged() {
            const audio = reading.audio;
            if (!audio || !audio.available) {
                return;
            }
            const changed = reading.lastPercent >= 0
                && (audio.percent !== reading.lastPercent || audio.muted !== reading.lastMuted);
            reading.lastPercent = audio.percent;
            reading.lastMuted = audio.muted;
            if (changed) {
                reading.show("volume");
            }
        }
    }
    onWatchingChanged: {
        lastPercent = audio && audio.available ? audio.percent : -1;
        lastMuted = audio ? audio.muted : false;
        lastBrightness = -1;
    }
    Component.onCompleted: {
        lastPercent = audio && audio.available ? audio.percent : -1;
        lastMuted = audio ? audio.muted : false;
    }

    // The first display's brightness, as Control Center's slider reads it.
    property real brightness: 0
    property real brightnessMax: 100
    property real lastBrightness: -1
    Loader {
        active: reading.watching
        sourceComponent: Brightness.ScreenBrightnessControl {
            id: control
            isSilent: true
            function read() {
                const displays = control.displays;
                if (!displays || displays.rowCount() < 1) {
                    return;
                }
                const index = displays.index(0, 0);
                const value = displays.data(index, displays.KItemModels.KRoleNames.role("brightness"));
                const most = displays.data(index, displays.KItemModels.KRoleNames.role("maxBrightness")) || 100;
                const changed = reading.lastBrightness >= 0 && value !== reading.lastBrightness;
                reading.brightness = value;
                reading.brightnessMax = most;
                reading.lastBrightness = value;
                if (changed) {
                    reading.show("brightness");
                }
            }
            readonly property Connections watch: Connections {
                target: control.displays
                function onDataChanged() { control.read(); }
                function onModelReset() { control.read(); }
                function onRowsInserted() { control.read(); }
            }
            Component.onCompleted: Qt.callLater(read)
        }
    }

    Item {
        id: mark
        width: reading.pitch
        height: parent.height

        SpeakerGlyph {
            anchors.centerIn: parent
            width: reading.iconSize + 4
            height: width
            visible: reading.kind === "volume"
            muted: reading.audio ? reading.audio.muted : false
            ink: reading.ink
        }
        SuiteIcon {
            anchors.centerIn: parent
            glyph: "sun"
            width: reading.iconSize
            height: reading.iconSize
            visible: reading.kind === "brightness"
        }
    }

    // How full, on a track as long as two of the row's marks.
    Rectangle {
        id: trackBar
        anchors.left: mark.right
        anchors.verticalCenter: parent.verticalCenter
        width: 34
        height: 4
        radius: 2
        color: reading.track

        Rectangle {
            width: Math.round(parent.width * reading.level)
            height: parent.height
            radius: parent.radius
            color: reading.ink
            Behavior on width {
                NumberAnimation { duration: reading.motionEnabled ? 90 : 0 }
            }
        }
    }
}
