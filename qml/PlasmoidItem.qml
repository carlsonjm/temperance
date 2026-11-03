/*
    SPDX-FileCopyrightText: 2015 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2022 ivan tkachenko <me@ratijas.tk>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

pragma ComponentBehavior: Bound

import QtQuick

import org.kde.kirigami as Kirigami

import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid

AbstractItem {
    id: plasmoidContainer

    property Item applet: model?.applet ?? null
    text: presentationText || applet?.Plasmoid.title || ""

    itemId: applet?.Plasmoid.pluginName ?? ""
    mainText: applet && (inVisibleLayout || (applet.toolTipMainText && applet.toolTipMainText != text)) ? applet.toolTipMainText : ""
    subText: applet?.toolTipSubText ?? ""
    mainItem: applet?.toolTipItem ?? null
    textFormat: applet?.toolTipTextFormat ?? 0 /* Text.AutoText, the default value */
    active: inVisibleLayout || (pointerMoved && systemTrayState.activeApplet !== applet && (text != mainText || subText.length > 0))
    inlineIcon: presentationIcon || applet?.Plasmoid.icon || "application-x-executable"

    // Plasma's own widgets read in sentence case, as Temperance's names do;
    // product names and words already capitalised inside stay as written.
    // Other languages keep their own casing.
    readonly property string pluginName: applet?.Plasmoid.pluginName ?? ""
    function sentenceCase(value) {
        if (!pluginName.startsWith("org.kde.") || !Qt.locale().name.startsWith("en")) return value;
        const kept = ["Bluetooth", "Plasma", "Wi-Fi"];
        return value.split(" ").map((word, i, words) => {
            if (i === 0 || kept.indexOf(word) >= 0 || words[i - 1] === "KDE"
                    || word.slice(1) !== word.slice(1).toLowerCase())
                return word;
            return word.charAt(0).toLowerCase() + word.slice(1);
        }).join(" ");
    }
    readonly property bool needsSetUp: applet?.Plasmoid.configurationRequired ?? false
    inlineName: {
        if (presentationText) return presentationText;
        // Weather, unset, titles itself "Set up Weather Report…": the tile
        // keeps the name and says what it needs underneath.
        if (pluginName === "org.kde.plasma.weather" && needsSetUp) return i18n("Weather report");
        return sentenceCase(suiteSentenceCaseLabel(text));
    }

    // The network in use, from the first connection the Networks widget
    // lists as "type: status". Its status names the network in the words
    // of Plasma's own translation; any other status is shown as it is.
    function networkState() {
        const lines = String(applet?.toolTipSubText ?? "").split("\n");
        const connection = lines.find(line => line.indexOf(": ") > 0);
        if (!connection) return { text: i18n("Not connected"), on: false };
        const status = connection.slice(connection.indexOf(": ") + 2);
        const marker = "@@NETWORK@@";
        const pattern = i18nd("plasmanetworkmanagement-libs", "Connected to %1", marker);
        const at = pattern.indexOf(marker);
        if (at >= 0) {
            const before = pattern.slice(0, at);
            const after = pattern.slice(at + marker.length);
            if (status.startsWith(before) && status.endsWith(after) && status.length > before.length + after.length)
                return { text: status.slice(before.length, status.length - after.length), on: true };
        }
        return { text: status, on: !(applet?.Plasmoid.busy ?? false) };
    }
    readonly property var appletState: {
        if (!applet) return { text: "", on: false };
        switch (pluginName) {
        case "org.kde.plasma.networkmanagement":
            return networkState();
        case "org.kde.plasma.bluetooth": {
            // Bluetooth's icon and status say it plainly: active when on,
            // "activated" while a device is connected.
            const on = applet.Plasmoid.status === PlasmaCore.Types.ActiveStatus;
            const connected = String(applet.Plasmoid.icon).indexOf("bluetooth-activated") >= 0;
            return { text: connected ? i18n("Connected") : on ? i18n("On") : i18n("Off"), on: on };
        }
        case "org.kde.kdeconnect": {
            const on = applet.Plasmoid.status === PlasmaCore.Types.ActiveStatus;
            return { text: on ? i18n("Connected") : i18n("No device"), on: on };
        }
        }
        if (needsSetUp) return { text: i18n("Not set up"), on: false };
        return { text: "", on: false };
    }
    inlineState: inlinePresentation ? appletState.text : ""
    inlineOn: appletState.on

    Kirigami.Icon {
        parent: plasmoidContainer.iconContainer
        anchors.fill: parent
        source: plasmoidContainer.presentationIcon
        visible: source !== ""
    }

    // FIXME: Use an input type agnostic way to activate whatever the primary
    // action of a plasmoid is supposed to be, even if it's just expanding the
    // Plasmoid. Not all plasmoids are supposed to expand and not all plasmoids
    // do anything with onActivated.
    onActivated: pos => {
        if (applet) {
            systemTrayState.setActiveApplet(applet)
            applet.Plasmoid.activated()
        }
    }

    onClicked: mouse => {
        if (!applet) {
            return
        }
        //forward click event to the applet
        const appletItem = applet.compactRepresentationItem ?? applet.fullRepresentationItem
        const mouseArea = findMouseArea(appletItem)

        if (mouseArea && mouse.button !== Qt.RightButton) {
            if (mouse.button === Qt.LeftButton) {
                systemTrayState.setActiveApplet(applet)
            }
            mouseArea.clicked(mouse)
        } else if (mouse.button === Qt.LeftButton) {//fallback
            activated(null)
        }
    }
    onPressed: mouse => {
        // Only Plasmoids can show context menu on the mouse pressed event.
        // SNI has few problems, for example legacy applications that still use XEmbed require mouse to be released.
        if (mouse.button === Qt.RightButton) {
            contextMenu(mouse);
        } else {
            const appletItem = applet.compactRepresentationItem ?? applet.fullRepresentationItem
            const mouseArea = findMouseArea(appletItem)
            if (mouseArea) {
                // The correct way here would be to invoke the "pressed"
                // signal; however, mouseArea.pressed signal is overridden
                // by its bool value, and our only option is to call the
                // handler directly.
                mouseArea.onPressed(mouse)
            }
        }
    }
    onContextMenu: mouse => {
        if (applet) {
            effectivePressed = false;
            Plasmoid.showPlasmoidMenu(applet, 0, inHiddenLayout ? applet.height : 0);
        }
    }
    onWheel: wheel => {
        if (!applet) {
            return
        }
        //forward wheel event to the applet
        const appletItem = applet.compactRepresentationItem ?? applet.fullRepresentationItem
        const mouseArea = findMouseArea(appletItem)
        if (mouseArea) {
            mouseArea.wheel(wheel)
        }
    }

    function __isSuitableMouseArea(child: Item): bool {
        const item = child.parent;
        return child instanceof MouseArea
            && child.enabled
            // check if MouseArea covers the entire item
            && (child.anchors.fill === item
                || (child.x === 0
                    && child.y === 0
                    && child.width === item.width
                    && child.height === item.height));
    }

    //some heuristics to find MouseArea
    function findMouseArea(item: Item): MouseArea {
        if (!item) {
            return null
        }

        if (item instanceof MouseArea) {
            return item as MouseArea
        }

        return item.children.find(__isSuitableMouseArea) ?? null;
    }

    //This is to make preloading effective, minimizes the scene changes
    function preloadFullRepresentationItem(fullRepresentationItem) {
        if (fullRepresentationItem && fullRepresentationItem.parent === null) {
            fullRepresentationItem.width = expandedRepresentation.width
            fullRepresentationItem.height = expandedRepresentation.height
            fullRepresentationItem.parent = preloadedStorage;
        }
    }

    onAppletChanged: {
        if (applet) {
            applet.parent = iconContainer
            applet.anchors.fill = applet.parent
            applet.visible = plasmoidContainer.presentationIcon === ""

            preloadFullRepresentationItem(applet.fullRepresentationItem)
        }
    }

    Connections {
        enabled: plasmoidContainer.applet !== null
        target: plasmoidContainer.findMouseArea(
            plasmoidContainer.applet?.compactRepresentationItem ??
            plasmoidContainer.applet?.fullRepresentationItem ??
            plasmoidContainer.applet
        )

        function onContainsPressChanged() {
            plasmoidContainer.effectivePressed = (target as MouseArea).containsPress;
        }

        // TODO For touch/stylus only, since the feature is not desired for mouse users
        function onPressAndHold(mouse) {
            if (mouse.button === Qt.LeftButton) {
                plasmoidContainer.contextMenu(mouse)
            }
        }
    }

    Connections {
        target: plasmoidContainer.applet?.Plasmoid ?? null

        //activation using global keyboard shortcut
        function onActivated() {
            plasmoidContainer.effectivePressed = true;
            Qt.callLater(() => {
                plasmoidContainer.effectivePressed = false;
            });
        }
    }

    Connections {
        target: plasmoidContainer.applet

        function onFullRepresentationItemChanged(fullRepresentationItem) {
            plasmoidContainer.preloadFullRepresentationItem(fullRepresentationItem)
        }

        function onExpandedChanged(expanded) {
            if (expanded) {
                plasmoidContainer.effectivePressed = false;
            }
        }
    }

    PlasmaComponents3.BusyIndicator {
        anchors {
            top: parent.top
            bottom: parent.bottom
            left: parent.left
            leftMargin: plasmoidContainer.inHiddenLayout ? Math.round(Kirigami.Units.smallSpacing / 2) : 0
            right: plasmoidContainer.inHiddenLayout ? undefined : parent.right
        }
        z: 999
        running: plasmoidContainer.applet?.Plasmoid.busy ?? false
    }

    Binding {
        property: "hideOnWindowDeactivate"
        value: !(Plasmoid.configuration.showPinButton && Plasmoid.configuration.pin)
        target: plasmoidContainer.applet
        when: plasmoidContainer.applet !== null
        restoreMode: Binding.RestoreBinding
    }
    Binding {
        property: "activeFocusOnTab"
        value: false
        target: plasmoidContainer.applet?.compactRepresentationItem ?? null
        when: plasmoidContainer?.applet && plasmoidContainer.inHiddenLayout
        restoreMode: Binding.RestoreBinding
    }
    Binding {
        property: "activeFocusOnTab"
        value: false
        target: plasmoidContainer.applet?.fullRepresentationItem ?? null
        when: plasmoidContainer?.applet && plasmoidContainer.inHiddenLayout && !plasmoidContainer.applet.compactRepresentationItem
        restoreMode: Binding.RestoreBinding
    }
}
