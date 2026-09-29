pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "../qml"

// The status icons sized for touch, compared by eye on the tablet. Icons a
// third larger and 44 apart looked disconnected; this round brings them
// closer, and 2, Closest, is what main.qml now draws. The Wi-Fi mark here is the fallback icon's
// symbolic drawing, bolder than the 24 px one a theme gives the live icon.
// In a row, the room between icons is each one's touch, so closer icons
// answer narrower touches, and the two candidates take the band's whole
// height back for it: a still contact at the bezel reaches the panel until
// it becomes a swipe. Each strip is at true size on Shuffle's 64 px band
// beside the dock's right end, with a line in the ticker to show the room it
// keeps. The bell and the clock are Temperance's own. An ordinary window:
// Esc or Close ends it, and it touches nothing of Temperance's.
//
//     qml6 StatusIconsBakeoff.qml [-- --image FILE] [--reach] [--grab FILE]
Window {
    id: win
    title: "Temperance bake-off: status icons closer"
    width: 1463
    height: 915
    visible: true
    color: "black"
    screen: Qt.application.screens.find(candidate => candidate.name === "eDP-1") ?? Qt.application.screens[0]
    visibility: Window.FullScreen

    function argument(name) {
        const at = Qt.application.arguments.indexOf(name);
        return at >= 0 && at + 1 < Qt.application.arguments.length ? Qt.application.arguments[at + 1] : "";
    }
    readonly property string imagePath: argument("--image")
    readonly property string grabPath: argument("--grab")
    property int backdrop: imagePath.length > 0 ? 0 : 1
    readonly property var backdropNames: ["Your wallpaper", "A bright wallpaper", "A white page"]
    property bool bandBlack: false
    property bool showReach: Qt.application.arguments.indexOf("--reach") >= 0
    readonly property real barHeight: 56
    readonly property real bandHeight: 64
    readonly property real stripHeight: Math.floor((height - barHeight) / styles.length)

    // Today's numbers are main.qml's. Glyph sizes are in logical pixels, as
    // the panel draws them. The network icon's is the box it is asked for:
    // at 22 the theme's own drawing shows a 16 px mark inside it, and at any
    // size it has no drawing for, its symbolic one fills the box, so 21 is a
    // mark a third larger.
    readonly property var styles: [
        { title: "Your pick: larger icons",
          detail: "As you chose it. Each icon's touch is wider than a fingertip needs, so they sit far apart.",
          target: 44, targetH: 44, gap: 0, edge: 0, lift: true,
          bell: 24, bellStroke: 2, weather: 21, temp: 10, network: 21,
          tray: 12, trayStroke: 2.5, bolt: 24, battery: 15 },
        { title: "1   Closer",
          detail: "Each touch a little narrower, still wider than an iPhone's smallest, and as tall as the band.",
          target: 38, targetH: 64, gap: 0, edge: 0, lift: true,
          bell: 24, bellStroke: 2, weather: 21, temp: 10, network: 21,
          tray: 12, trayStroke: 2.5, bolt: 24, battery: 15 },
        { title: "2   Closest",
          detail: "Each touch a hair under an iPhone's smallest in width, made up by being as tall as the band.",
          target: 34, targetH: 64, gap: 0, edge: 0, lift: true,
          bell: 24, bellStroke: 2, weather: 21, temp: 10, network: 21,
          tray: 12, trayStroke: 2.5, bolt: 24, battery: 15 }
    ]

    readonly property color text: "#F8F8FF"
    readonly property color text2: Qt.rgba(1, 1, 1, 0.66)
    readonly property var dockApps: ["com.mitchellh.ghostty", "zen-browser", "org.kde.dolphin",
        "code-oss", "org.kde.kate", "org.kde.elisa", "org.kde.konsole"]
    readonly property real dockGlyph: 44
    readonly property real dockGap: 8
    readonly property real dockWidth: dockApps.length * dockGlyph + (dockApps.length - 1) * dockGap
    readonly property date shownTime: new Date()

    Shortcut {
        sequence: "Escape"
        onActivated: Qt.quit()
    }

    // ---------------------------------------------------------------- backdrop

    Image {
        anchors.fill: parent
        visible: win.backdrop === 0
        source: win.imagePath.length > 0 ? "file://" + win.imagePath : ""
        fillMode: Image.PreserveAspectCrop
        asynchronous: false
    }
    Rectangle {
        anchors.fill: parent
        visible: win.backdrop === 1
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: "#f3d9b1" }
            GradientStop { position: 0.5; color: "#bfe3f2" }
            GradientStop { position: 1; color: "#f6f1e7" }
        }
    }
    Rectangle {
        anchors.fill: parent
        visible: win.backdrop === 2
        color: "white"
    }

    // ---------------------------------------------------------------- pieces

    // One status icon's touch: invisible, the size it answers to. Under the
    // finger its glyph lifts where the candidate says so; today's only grows
    // under a hovering pointer, which a finger is not.
    component Target: Item {
        id: target
        required property var style
        required property string name
        property real wide: style.target
        signal touched(string name)
        default property alias glyph: face.data

        Layout.preferredWidth: wide
        Layout.minimumWidth: wide
        Layout.maximumWidth: wide
        Layout.preferredHeight: style.targetH
        Layout.alignment: Qt.AlignVCenter

        readonly property bool pressed: tap.pressed
        property real lift: style.lift && pressed ? 1 : 0
        Behavior on lift {
            id: liftMotion
            NumberAnimation { duration: liftMotion.targetValue > 0 ? 180 : 140; easing.type: Easing.OutCubic }
        }

        Item {
            id: face
            anchors.fill: parent
            scale: 1 + target.lift * 0.12 + (hover.hovered && !target.style.lift ? 0.06 : 0)
            Behavior on scale { enabled: !target.style.lift; NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        }
        Rectangle {
            anchors.fill: parent
            visible: win.showReach
            color: Qt.rgba(1, 1, 1, target.pressed ? 0.16 : 0.06)
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.35)
            radius: 6
        }
        HoverHandler { id: hover }
        TapHandler {
            id: tap
            onTapped: target.touched(target.name)
        }
    }

    // Temperance's tray mark, drawn as main.qml draws it at rest.
    component TrayMark: Canvas {
        id: mark
        property real size: 9
        property real stroke: 2
        width: Math.round(size * 28 / 9)
        height: width
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            const cx = width / 2;
            const cy = height / 2;
            const vertices = [[cx, cy - size * 0.55], [cx + size * 0.6, cy + size * 0.4], [cx - size * 0.6, cy + size * 0.4]];
            const inset = size * 0.16;
            ctx.beginPath();
            for (let i = 0; i < 3; ++i) {
                const v = vertices[i];
                const prev = vertices[(i + 2) % 3];
                const next = vertices[(i + 1) % 3];
                const a = inset / Math.hypot(prev[0] - v[0], prev[1] - v[1]);
                const b = inset / Math.hypot(next[0] - v[0], next[1] - v[1]);
                const x = v[0] + (prev[0] - v[0]) * a;
                const y = v[1] + (prev[1] - v[1]) * a;
                if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
                ctx.quadraticCurveTo(v[0], v[1], v[0] + (next[0] - v[0]) * b, v[1] + (next[1] - v[1]) * b);
            }
            ctx.closePath();
            ctx.lineJoin = "round";
            ctx.fillStyle = win.text;
            ctx.strokeStyle = win.text;
            ctx.lineWidth = stroke;
            ctx.fill();
            ctx.stroke();
        }
    }

    // One candidate: its name, and the band as it stands at the foot of the
    // tablet, with the dock at the true centre and Temperance to its right.
    Column {
        anchors.top: parent.top
        width: parent.width

        Repeater {
            model: win.styles
            delegate: Item {
                id: strip
                required property var modelData
                required property int index
                readonly property var style: modelData
                property string lastTouch: ""
                width: win.width
                height: win.stripHeight

                Column {
                    x: 20
                    y: 18
                    width: 620
                    spacing: 3
                    Text {
                        text: strip.style.title
                        color: win.text
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        style: Text.Raised
                        styleColor: Qt.rgba(0, 0, 0, 0.35)
                    }
                    Text {
                        width: parent.width
                        text: strip.style.detail
                        color: win.text
                        opacity: 0.8
                        wrapMode: Text.WordWrap
                        font.pixelSize: 13
                        style: Text.Raised
                        styleColor: Qt.rgba(0, 0, 0, 0.35)
                    }
                    Text {
                        text: strip.lastTouch.length > 0 ? "Your last touch opened " + strip.lastTouch + "."
                            : "Touch an icon to see which one answers."
                        color: win.text
                        opacity: 0.8
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        style: Text.Raised
                        styleColor: Qt.rgba(0, 0, 0, 0.35)
                    }
                }

                Item {
                    id: band
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: win.bandHeight

                    // Shuffle's band at rest: its fade, clear at the top and
                    // 92% black at the screen's edge; or its blackout.
                    Rectangle {
                        anchors.fill: parent
                        visible: !win.bandBlack
                        gradient: Gradient {
                            GradientStop { position: 0 / 24; color: Qt.rgba(0, 0, 0, 0.0000) }
                            GradientStop { position: 1 / 24; color: Qt.rgba(0, 0, 0, 0.0076) }
                            GradientStop { position: 2 / 24; color: Qt.rgba(0, 0, 0, 0.0285) }
                            GradientStop { position: 3 / 24; color: Qt.rgba(0, 0, 0, 0.0598) }
                            GradientStop { position: 4 / 24; color: Qt.rgba(0, 0, 0, 0.0994) }
                            GradientStop { position: 5 / 24; color: Qt.rgba(0, 0, 0, 0.1453) }
                            GradientStop { position: 6 / 24; color: Qt.rgba(0, 0, 0, 0.1960) }
                            GradientStop { position: 7 / 24; color: Qt.rgba(0, 0, 0, 0.2500) }
                            GradientStop { position: 8 / 24; color: Qt.rgba(0, 0, 0, 0.3061) }
                            GradientStop { position: 9 / 24; color: Qt.rgba(0, 0, 0, 0.3632) }
                            GradientStop { position: 10 / 24; color: Qt.rgba(0, 0, 0, 0.4205) }
                            GradientStop { position: 11 / 24; color: Qt.rgba(0, 0, 0, 0.4772) }
                            GradientStop { position: 12 / 24; color: Qt.rgba(0, 0, 0, 0.5324) }
                            GradientStop { position: 13 / 24; color: Qt.rgba(0, 0, 0, 0.5857) }
                            GradientStop { position: 14 / 24; color: Qt.rgba(0, 0, 0, 0.6365) }
                            GradientStop { position: 15 / 24; color: Qt.rgba(0, 0, 0, 0.6843) }
                            GradientStop { position: 16 / 24; color: Qt.rgba(0, 0, 0, 0.7286) }
                            GradientStop { position: 17 / 24; color: Qt.rgba(0, 0, 0, 0.7691) }
                            GradientStop { position: 18 / 24; color: Qt.rgba(0, 0, 0, 0.8053) }
                            GradientStop { position: 19 / 24; color: Qt.rgba(0, 0, 0, 0.8371) }
                            GradientStop { position: 20 / 24; color: Qt.rgba(0, 0, 0, 0.8642) }
                            GradientStop { position: 21 / 24; color: Qt.rgba(0, 0, 0, 0.8862) }
                            GradientStop { position: 22 / 24; color: Qt.rgba(0, 0, 0, 0.9030) }
                            GradientStop { position: 23 / 24; color: Qt.rgba(0, 0, 0, 0.9143) }
                            GradientStop { position: 24 / 24; color: Qt.rgba(0, 0, 0, 0.9200) }
                        }
                    }
                    Rectangle {
                        anchors.fill: parent
                        visible: win.bandBlack
                        color: "#141414"
                    }

                    Row {
                        id: dock
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: win.dockGap
                        Repeater {
                            model: win.dockApps
                            delegate: Kirigami.Icon {
                                required property string modelData
                                width: win.dockGlyph
                                height: win.dockGlyph
                                source: modelData
                            }
                        }
                    }

                    // Temperance in the right flank: from the dock's edge to
                    // 20 from the screen's, as the band places it.
                    Item {
                        x: dock.x + win.dockWidth + 8
                        width: parent.width - 20 - x
                        height: parent.height

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 4
                            anchors.rightMargin: strip.style.edge
                            spacing: strip.style.gap

                            Text {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                Layout.rightMargin: 4 - strip.style.gap
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                                text: "Zen Browser  ·  Download complete: Shuffle-1.0-release-notes.pdf"
                                color: win.text
                                font.pixelSize: 13
                            }

                            Target {
                                style: strip.style
                                name: "notifications"
                                onTouched: name => strip.lastTouch = name
                                BellGlyph {
                                    width: strip.style.bell
                                    height: strip.style.bell
                                    anchors.centerIn: parent
                                    anchors.verticalCenterOffset: 1.8 * strip.style.bell / 18
                                    strokeWidth: strip.style.bellStroke
                                    glyphColor: win.text
                                }
                            }

                            Target {
                                style: strip.style
                                name: "Weather"
                                onTouched: name => strip.lastTouch = name
                                Kirigami.Icon {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.horizontalCenterOffset: -1
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: strip.style.weather
                                    height: strip.style.weather
                                    source: "weather-few-clouds-symbolic"
                                    // Kirigami would round 21 back to 16.
                                    roundToIconSize: false
                                    color: win.text
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    y: parent.height / 2 + 6 * strip.style.weather / 16 + (12 * strip.style.weather / 16 - height) / 2
                                    text: "61°"
                                    color: win.text
                                    font.pixelSize: strip.style.temp
                                    font.weight: Font.Medium
                                }
                            }

                            Target {
                                style: strip.style
                                name: "Control Center"
                                onTouched: name => strip.lastTouch = name
                                Kirigami.Icon {
                                    anchors.centerIn: parent
                                    width: strip.style.network
                                    height: strip.style.network
                                    source: "network-wireless-signal-excellent-symbolic"
                                    // Off the standard sizes the theme can answer
                                    // with an icon Kirigami does not tint.
                                    roundToIconSize: false
                                    isMask: true
                                    color: win.text
                                }
                            }

                            Target {
                                style: strip.style
                                name: "the tray"
                                onTouched: name => strip.lastTouch = name
                                TrayMark {
                                    anchors.centerIn: parent
                                    anchors.verticalCenterOffset: 1
                                    size: strip.style.tray
                                    stroke: strip.style.trayStroke
                                }
                            }

                            Target {
                                style: strip.style
                                name: "Control Center, from the battery"
                                wide: Math.max(strip.style.target, batteryText.implicitWidth + 4)
                                onTouched: name => strip.lastTouch = name
                                Text {
                                    id: batteryText
                                    anchors.centerIn: parent
                                    text: "86%"
                                    color: win.text
                                    font.pixelSize: strip.style.battery
                                    font.weight: Font.Medium
                                }
                            }

                            StatusClock {
                                Layout.leftMargin: 6
                                Layout.rightMargin: 6
                                Layout.preferredWidth: implicitWidth
                                Layout.minimumWidth: implicitWidth
                                Layout.maximumWidth: implicitWidth
                                Layout.preferredHeight: 42
                                Layout.alignment: Qt.AlignVCenter
                                dateTime: win.shownTime
                                TapHandler { onTapped: strip.lastTouch = "the calendar" }
                            }
                        }
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 2
                    color: "black"
                    visible: strip.index < win.styles.length - 1
                }
            }
        }
    }

    // ---------------------------------------------------------------- controls

    component Pill: Rectangle {
        id: pillButton
        property string label
        signal activated()
        height: 38
        width: pillText.implicitWidth + 34
        radius: 19
        color: tap.pressed ? Qt.rgba(1, 1, 1, 0.26) : Qt.rgba(1, 1, 1, 0.12)
        Text {
            id: pillText
            anchors.centerIn: parent
            text: pillButton.label
            color: "white"
            font.pixelSize: 14
            font.weight: Font.Medium
        }
        TapHandler {
            id: tap
            onTapped: pillButton.activated()
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: win.barHeight
        color: Qt.rgba(12 / 255, 12 / 255, 14 / 255, 0.96)
        Row {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12
            Pill {
                label: win.backdropNames[win.backdrop]
                onActivated: win.backdrop = (win.backdrop + 1) % 3
            }
            Pill {
                label: win.bandBlack ? "Band: black, a window touching" : "Band: clear"
                onActivated: win.bandBlack = !win.bandBlack
            }
            Pill {
                label: win.showReach ? "Hide where a touch lands" : "Show where a touch lands"
                onActivated: win.showReach = !win.showReach
            }
        }
        Pill {
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            label: "Close"
            onActivated: Qt.quit()
        }
    }

    // A picture of the window, then it closes: for checking the preview in a
    // private compositor before it is shown.
    Timer {
        running: win.grabPath.length > 0
        interval: 3000
        onTriggered: win.contentItem.grabToImage(result => {
            result.saveToFile(win.grabPath);
            Qt.quit();
        })
    }
}
