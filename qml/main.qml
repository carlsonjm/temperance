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
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import org.kde.plasma.workspace.dbus as DBus

ContainmentItem {
    id: root

    readonly property bool vertical: Plasmoid.formFactor === PlasmaCore.Types.Vertical
    readonly property int itemSize: Kirigami.Units.iconSizes.smallMedium
    readonly property bool oneRowOrColumn: true
    readonly property alias systemTrayState: systemTrayState
    readonly property alias hiddenLayout: expandedRepresentation.hiddenLayout
    readonly property alias hiddenModel: hiddenModel
    readonly property alias organizedTrayModel: organizedTrayModel
    readonly property alias controlCenterModel: controlCenterModel
    readonly property alias notificationHistoryModel: groupedNotificationHistory
    property var appletsById: ({})
    readonly property bool previewMode: Qt.application.name === "plasmawindowed"
    readonly property color accentColor: Plasmoid.configuration.accentColor || "#00F2BA"
    property bool demoNotificationVisible: false
    property int demoNotificationIndex: 0
    property int lastLiveNotificationCount: 0
    property real notificationPopupHeightLimit: 600
    property var presentedNotificationKeys: ({})

    function nextUnpresentedNotification() {
        for (let row = 0; row < railNotifications.count; ++row) {
            const idx = railNotifications.index(row, 0);
            if (!presentedNotificationKeys[priorityNotificationKey(railNotifications, idx)])
                return row;
        }
        return -1;
    }
    property int pendingNotificationCount: 0
    property int notificationAutoBatchSize: 0
    property bool notificationCopyActive: false
    property bool notificationSequenceActive: false
    property bool notificationReviewComplete: false
    property var minimizedPriorityNotifications: ({})
    readonly property int compactWidth: Math.max(320,
        Math.min(600, Number(Plasmoid.configuration.compactWidth) || 400))
    readonly property bool adaptiveWidth: Plasmoid.configuration.adaptiveWidth !== false
    // Only the controls establish a floor. The ticker receives whatever live
    // gap remains between the task dock and this applet's right edge.
    readonly property int responsiveMinimumWidth: 10
        + 26 // network
        + 26 // tray
        + (notificationsEnabled ? 34 : 0)
        + (weatherEnabled ? 26 : 0)
        + (hasBattery ? 26 : 0)
        + Math.max(0, (2 + (notificationsEnabled ? 1 : 0)
            + (weatherEnabled ? 1 : 0) + (hasBattery ? 1 : 0) - 1) * 2)
    property int responsiveMeasuredWidth: responsiveMinimumWidth
    readonly property bool notificationsEnabled: Plasmoid.configuration.showNotifications !== false
    readonly property bool weatherEnabled: Plasmoid.configuration.showWeather !== false
    readonly property int priorityAlertFreshnessMs: Math.max(5,
        Math.min(60, Number(Plasmoid.configuration.priorityAlertDuration) || 20)) * 1000
    readonly property string temperatureUnit: {
        const configured = String(Plasmoid.configuration.temperatureUnit || "fahrenheit");
        return ["weather", "fahrenheit", "celsius"].includes(configured)
            ? configured : "fahrenheit";
    }
    property string currentTemperature: ""
    property bool weatherRequestPending: false
    readonly property bool hasBattery: Boolean(compactBattery.properties.IsPresent)
    readonly property var demoNotificationTexts: [
        i18n("FINAL SCROLL TEST: This message is intentionally much longer than the notification rail can display at once. Hover the paging arrows and keep reading while the remaining sentence glides cleanly into view."),
        i18n("System Update: Packages are ready"),
        i18n("Downloads: Transfer completed")
    ]
    readonly property bool hasAttention: notificationsEnabled
        && (railNotifications.count > 0 || demoNotificationVisible)

    function priorityNotificationKey(model, modelIndex) {
        const notificationId = model.data(modelIndex,
            NotificationManager.Notifications.IdRole);
        const created = model.data(modelIndex,
            NotificationManager.Notifications.CreatedRole);
        return String(notificationId) + "|" + String(created);
    }

    function isPriorityNotificationMinimized(model, modelIndex) {
        return Boolean(minimizedPriorityNotifications[
            priorityNotificationKey(model, modelIndex)]);
    }

    function isPriorityNotificationCandidate(model, modelIndex) {
        const urgency = model.data(modelIndex,
            NotificationManager.Notifications.UrgencyRole);
        // An action makes a message interactive, not urgent.
        return urgency === NotificationManager.Notifications.CriticalUrgency
            || isFreshLogoutCancellation(model, modelIndex);
    }

    function minimizePriorityNotification(modelIndex) {
        const minimized = Object.assign({}, minimizedPriorityNotifications);
        minimized[priorityNotificationKey(notificationHistory, modelIndex)] = true;
        minimizedPriorityNotifications = minimized;
        priorityNotifications.invalidateFilter();
        railNotifications.invalidateFilter();
    }

    component RailTicker: Item {
        id: ticker
        required property string text
        property bool scrollingEnabled: true
        property bool alignRight: false
        property bool exposeOverflow: false
        implicitWidth: tickerLabel.implicitWidth
        implicitHeight: tickerLabel.implicitHeight
        Layout.preferredWidth: implicitWidth
        Layout.minimumWidth: 0
        clip: !exposeOverflow

        readonly property real overflow: Math.max(0, tickerLabel.implicitWidth - width)
        readonly property real contentWidth: tickerLabel.implicitWidth
        readonly property real restingX: alignRight ? Math.max(0, width - tickerLabel.implicitWidth) : 0

        function syncScroll() {
            scrollDelay.stop();
            tickerScroll.stop();
            tickerLabel.x = restingX;
            if (scrollingEnabled && overflow > 2) scrollDelay.restart();
        }

        PlasmaComponents.Label {
            id: tickerLabel
            x: 0
            anchors.verticalCenter: parent.verticalCenter
            text: ticker.text
            textFormat: Text.PlainText
            maximumLineCount: 1
            Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
        }

        onTextChanged: {
            tickerLabel.x = restingX;
            tickerLabel.opacity = 0;
            tickerReveal.restart();
            Qt.callLater(ticker.syncScroll);
        }
        onScrollingEnabledChanged: syncScroll()
        onOverflowChanged: resizeSettle.restart()
        SequentialAnimation {
            id: tickerReveal
            PauseAnimation { duration: 30 }
            NumberAnimation { target: tickerLabel; property: "opacity"; to: 1; duration: 180; easing.type: Easing.OutCubic }
        }

        Timer {
            id: scrollDelay
            interval: 1800
            repeat: false
            onTriggered: {
                if (ticker.scrollingEnabled && ticker.overflow > 2) tickerScroll.restart();
            }
        }

        Timer {
            id: resizeSettle
            interval: 230
            repeat: false
            onTriggered: ticker.syncScroll()
        }

        SequentialAnimation {
            id: tickerScroll
            NumberAnimation {
                target: tickerLabel
                property: "x"
                from: ticker.restingX
                to: -ticker.overflow
                duration: Math.max(1450, ticker.overflow * 20)
                easing.type: Easing.Linear
            }
            PauseAnimation { duration: 1400 }
            NumberAnimation {
                target: tickerLabel
                property: "x"
                from: -ticker.overflow
                to: ticker.restingX
                duration: 420
                easing.type: Easing.OutCubic
            }
            // One deliberate read per reveal. Leaving and returning to the
            // control is the explicit gesture that makes it readable again.
        }
    }

    component RailControlButton: PlasmaComponents.ToolButton {
        id: railControlButton
        property color glyphColor: railControlButton.pressed ? Qt.lighter(root.accentColor, 1.18)
            : railControlButton.hovered ? root.accentColor : "#F8F8FF"
        implicitWidth: 26
        implicitHeight: 32
        display: PlasmaComponents.AbstractButton.IconOnly
        Behavior on glyphColor { ColorAnimation { duration: 120 } }
        background: null
        contentItem: Item {
            BellGlyph {
                width: 18
                height: 18
                anchors.centerIn: parent
                // Keep the same optical baseline when the clapper returns.
                anchors.verticalCenterOffset: 1.8
                glyphColor: railControlButton.glyphColor
                clapperProgress: root.hasAttention ? 0 : 1
            }
        }
    }

    component RailChevronButton: PlasmaComponents.ToolButton {
        id: railChevronButton
        required property bool pointsLeft
        property color glyphColor: railChevronButton.pressed ? Qt.lighter(root.accentColor, 1.18)
            : railChevronButton.hovered ? root.accentColor : "#F8F8FF"
        implicitWidth: 26
        implicitHeight: 26
        background: null
        Behavior on glyphColor { ColorAnimation { duration: 120 } }
        contentItem: Canvas {
            id: chevronGlyphCanvas
            implicitWidth: 10
            implicitHeight: 14
            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                ctx.strokeStyle = railChevronButton.glyphColor;
                ctx.lineWidth = 1.8;
                ctx.lineCap = "round";
                ctx.lineJoin = "round";
                ctx.beginPath();
                if (railChevronButton.pointsLeft) {
                    ctx.moveTo(width * 0.65, height * 0.2);
                    ctx.lineTo(width * 0.35, height * 0.5);
                    ctx.lineTo(width * 0.65, height * 0.8);
                } else {
                    ctx.moveTo(width * 0.35, height * 0.2);
                    ctx.lineTo(width * 0.65, height * 0.5);
                    ctx.lineTo(width * 0.35, height * 0.8);
                }
                ctx.stroke();
            }
            Connections {
                target: railChevronButton
                function onGlyphColorChanged() { chevronGlyphCanvas.requestPaint(); }
            }
        }
    }

    component StatusIconButton: Item {
        id: statusIconButton
        required property var statusIcon
        required property string accessibleName
        property bool active: false
        signal triggered()
        Layout.preferredWidth: 26
        Layout.minimumWidth: 26
        Layout.maximumWidth: 26
        Layout.fillHeight: true
        Layout.topMargin: 4
        Layout.bottomMargin: 4
        Accessible.name: accessibleName
        Accessible.role: Accessible.Button

        Kirigami.Icon {
            anchors.centerIn: parent
            source: statusIconButton.statusIcon
            implicitWidth: root.itemSize
            implicitHeight: implicitWidth
            color: "#F8F8FF"
            opacity: 1
            scale: statusIconButtonHover.hovered ? 1.06 : 1
            Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        }
        HoverHandler { id: statusIconButtonHover }
        TapHandler { onTapped: statusIconButton.triggered() }
        PlasmaComponents.ToolTip { text: statusIconButton.accessibleName }
    }

    implicitWidth: adaptiveWidth ? responsiveMeasuredWidth : compactWidth
    implicitHeight: 42
    Layout.fillWidth: false
    Layout.minimumWidth: adaptiveWidth ? responsiveMinimumWidth : compactWidth
    // Responsive mode follows actual neighboring applet geometry. It has no
    // configured, display-derived, or percentage-based width target.
    Layout.preferredWidth: adaptiveWidth ? responsiveMeasuredWidth : compactWidth
    Layout.maximumWidth: adaptiveWidth ? responsiveMeasuredWidth : compactWidth
    Layout.minimumHeight: 42
    Layout.preferredHeight: 42
    Layout.maximumHeight: 42

    function refreshResponsiveWidth() {
        notificationPopupHeightLimit = Plasmoid.availablePopupHeight(root);
        if (!adaptiveWidth) return;
        const measured = Plasmoid.availablePanelWidth(root, responsiveMinimumWidth, 6);
        if (Math.abs(measured - responsiveMeasuredWidth) > 1)
            responsiveMeasuredWidth = measured;
    }

    onAdaptiveWidthChanged: Qt.callLater(refreshResponsiveWidth)
    onResponsiveMinimumWidthChanged: Qt.callLater(refreshResponsiveWidth)
    Timer {
        interval: 120
        repeat: true
        running: root.visible
        onTriggered: root.refreshResponsiveWidth()
    }

    function registerApplet(itemId, applet) {
        if (!itemId || !applet) return;
        const next = Object.assign({}, appletsById);
        next[itemId] = applet;
        appletsById = next;
    }

    function unregisterApplet(itemId, applet) {
        if (!itemId || appletsById[itemId] !== applet) return;
        const next = Object.assign({}, appletsById);
        delete next[itemId];
        appletsById = next;
    }

    function activateAppletById(itemId) {
        const applet = appletsById[itemId];
        if (applet && systemTrayState.expanded && systemTrayState.activeApplet === applet) {
            systemTrayState.expanded = false;
            return;
        }
        if (applet) systemTrayState.setActiveApplet(applet);
    }

    function openSurface(page) {
        if (systemTrayState.expanded && !systemTrayState.activeApplet
                && systemTrayState.page === page) {
            systemTrayState.expanded = false;
            return;
        }
        systemTrayState.setActiveApplet(null);
        systemTrayState.page = page;
        systemTrayState.expanded = true;
    }

    function openNotifications() {
        notificationHistory.lastRead = new Date();
        openSurface("notifications");
    }

    function plainNotificationText(value) {
        return String(value || "")
            .replace(/<br\s*\/?\s*>/gi, " ")
            .replace(/<[^>]*>/g, " ")
            .replace(/&nbsp;/gi, " ")
            .replace(/&amp;/gi, "&")
            .replace(/&lt;/gi, "<")
            .replace(/&gt;/gi, ">")
            .replace(/\s+/g, " ")
            .trim();
    }

    function notificationDisplayText(applicationName, summary, body) {
        const app = plainNotificationText(applicationName);
        const title = plainNotificationText(summary);
        const detail = plainNotificationText(body);
        let message = title;
        if (detail && detail.toLowerCase() !== title.toLowerCase()) {
            message += (message ? " · " : "") + detail;
        }
        return app ? app + (message ? ": " + message : "") : message;
    }

    function isFreshNotification(model, modelIndex) {
        const occurred = model.data(modelIndex, NotificationManager.Notifications.UpdatedRole)
            || model.data(modelIndex, NotificationManager.Notifications.CreatedRole);
        const occurredMs = occurred && occurred.getTime
            ? occurred.getTime() : Date.parse(String(occurred));
        if (!Number.isFinite(occurredMs)) return false;
        const age = Date.now() - occurredMs;
        return age >= -5000 && age < priorityAlertFreshnessMs;
    }

    function isFreshLogoutCancellation(model, modelIndex) {
        if (!isFreshNotification(model, modelIndex)) return false;
        const alertText = (String(model.data(modelIndex,
            NotificationManager.Notifications.SummaryRole) || "") + " "
            + String(model.data(modelIndex,
                NotificationManager.Notifications.BodyRole) || "")).toLowerCase();
        return alertText.includes("logout canceled")
            || alertText.includes("logout cancelled")
            || (alertText.includes("logout") && alertText.includes("cancel"));
    }

    function clearNotificationHistory() {
        demoNotificationVisible = false;
        notificationBatchTimer.stop();
        nextNotificationIntro.stop();
        notificationSequenceActive = false;
        pendingNotificationCount = 0;
        notificationAutoBatchSize = 0;
        notificationReviewComplete = false;
        notificationIntro.stop();
        notificationReveal.stop();
        notificationHide.stop();
        notificationCopyActive = false;
        notificationMessageLayer.opacity = 0;
        // Clear is an explicit dismissal, including active/persistent alerts.
        // Use the ungrouped model and walk backward as removals shift rows.
        for (let row = notificationHistory.count - 1; row >= 0; --row) {
            notificationHistory.close(notificationHistory.index(row, 0));
        }
    }

    function acknowledgeNotifications() {
        if (railNotifications.count > 0) {
            // "Seen" removes alerts from the quiet rail while retaining them in
            // notification history. Clearing history remains a separate action.
            notificationHistory.lastRead = new Date();
        } else {
            demoNotificationVisible = false;
        }
        notificationBatchTimer.stop();
        nextNotificationIntro.stop();
        notificationSequenceActive = false;
        pendingNotificationCount = 0;
        notificationAutoBatchSize = 0;
        notificationReviewComplete = false;
        notificationIntro.stop();
        notificationReveal.stop();
        notificationHide.stop();
        notificationCopyActive = false;
        notificationMessageLayer.opacity = 0;
    }

    function playNotificationIntro() {
        if (!hasAttention) return;
        notificationReveal.stop();
        notificationHide.stop();
        notificationIntro.restart();
    }

    function revealNotificationText() {
        if (!hasAttention) return;
        notificationBatchTimer.stop();
        nextNotificationIntro.stop();
        notificationSequenceActive = false;
        notificationIntro.stop();
        notificationHide.stop();
        notificationReveal.restart();
    }

    function hideNotificationText() {
        if (!hasAttention) return;
        notificationIntro.stop();
        notificationReveal.stop();
        notificationHide.restart();
    }

    function acHoldingCharge(onBattery, state) {
        // UPower: fully charged (4) or pending charge (5), with AC confirmed.
        return onBattery === false && (Number(state) === 4 || Number(state) === 5);
    }

    readonly property bool batteryOnAC: acHoldingCharge(compactPower.properties.OnBattery,
        compactBattery.properties.State)

    function batteryLabel() {
        if (!hasBattery) return "";
        const rawPercentage = compactBattery.properties.Percentage;
        if (rawPercentage !== undefined && rawPercentage !== null && rawPercentage !== "") {
            const percentage = Number(rawPercentage);
            if (Number.isFinite(percentage) && percentage >= 0 && percentage <= 100)
                return Math.round(percentage) + "%";
        }
        const batteryApplet = appletsById["org.kde.plasma.battery"];
        if (batteryApplet) {
            const tooltip = String(batteryApplet.toolTipMainText || "") + " "
                + String(batteryApplet.toolTipSubText || "");
            const displayedPercentage = tooltip.match(/([0-9]{1,3})\s*%/);
            if (displayedPercentage) return displayedPercentage[1] + "%";
        }
        return "—";
    }

    function networkIcon() {
        const network = appletsById["org.kde.plasma.networkmanagement"];
        return network && network.Plasmoid && network.Plasmoid.icon
            ? network.Plasmoid.icon : "network-wireless-signal-excellent-symbolic";
    }

    function appletIcon(itemId, fallback) {
        const applet = appletsById[itemId];
        return applet && applet.Plasmoid && applet.Plasmoid.icon
            ? applet.Plasmoid.icon : fallback;
    }

    function weatherText() {
        if (currentTemperature) return currentTemperature;
        const weather = appletsById["org.kde.plasma.weather"];
        if (!weather) return "—°";
        const observation = weather.fullRepresentationItem
            ? weather.fullRepresentationItem.lastObservation : null;
        const nativeTemperature = observation ? Number(observation.temperature) : NaN;
        if (Number.isFinite(nativeTemperature)) return Math.round(nativeTemperature) + "°";
        const tooltip = String(weather.toolTipSubText || "").replace(/<[^>]*>/g, " ");
        const match = tooltip.match(/-?[0-9]+(?:\.[0-9]+)?\s*°/);
        return match ? match[0].replace(/\s+/g, "") : "—°";
    }

    function fetchCurrentTemperature(latitude, longitude) {
        const request = new XMLHttpRequest();
        request.onreadystatechange = function() {
            if (request.readyState !== XMLHttpRequest.DONE) return;
            root.weatherRequestPending = false;
            if (request.status < 200 || request.status >= 300) return;
            try {
                const payload = JSON.parse(request.responseText);
                const temperature = Number(payload.current.temperature_2m);
                if (Number.isFinite(temperature)) root.currentTemperature = Math.round(temperature) + "°";
            } catch (error) {
                console.warn("Temperance: unable to parse current weather", error);
            }
        };
        const apiTemperatureUnit = temperatureUnit === "celsius" ? "celsius" : "fahrenheit";
        request.open("GET", "https://api.open-meteo.com/v1/forecast?latitude="
            + encodeURIComponent(latitude) + "&longitude=" + encodeURIComponent(longitude)
            + "&current=temperature_2m&temperature_unit=" + apiTemperatureUnit, true);
        request.send();
    }

    function requestCurrentTemperature() {
        if (weatherRequestPending) return;
        const weather = appletsById["org.kde.plasma.weather"];
        if (!weather) return;
        const page = weather.fullRepresentationItem;
        const observation = page ? page.lastObservation : null;
        const nativeTemperature = observation ? Number(observation.temperature) : NaN;
        if (temperatureUnit === "weather" && Number.isFinite(nativeTemperature)) {
            currentTemperature = Math.round(nativeTemperature) + "°";
            return;
        }
        if (!Plasmoid.configuration.useOnlineWeatherFallback) return;

        const station = page ? page.station : null;
        const latitude = station ? Number(station.latitude) : NaN;
        const longitude = station ? Number(station.longitude) : NaN;
        weatherRequestPending = true;
        if (Number.isFinite(latitude) && Number.isFinite(longitude)) {
            fetchCurrentTemperature(latitude, longitude);
            return;
        }

        // Some providers expose only a display location. Resolve that once,
        // then use the same lightweight current-temperature request.
        const location = String(weather.toolTipMainText || "").trim();
        if (!location) {
            weatherRequestPending = false;
            return;
        }
        const geocode = new XMLHttpRequest();
        geocode.onreadystatechange = function() {
            if (geocode.readyState !== XMLHttpRequest.DONE) return;
            if (geocode.status < 200 || geocode.status >= 300) {
                root.weatherRequestPending = false;
                return;
            }
            try {
                const payload = JSON.parse(geocode.responseText);
                const result = payload.results && payload.results.length ? payload.results[0] : null;
                if (result) {
                    root.fetchCurrentTemperature(Number(result.latitude), Number(result.longitude));
                    return;
                }
            } catch (error) {
                console.warn("Temperance: unable to resolve weather location", error);
            }
            root.weatherRequestPending = false;
        };
        geocode.open("GET", "https://geocoding-api.open-meteo.com/v1/search?count=1&language=en&format=json&name="
            + encodeURIComponent(location), true);
        geocode.send();
    }

    Timer {
        interval: root.currentTemperature ? 600000 : 5000
        running: root.weatherEnabled
        repeat: true
        triggeredOnStart: true
        onTriggered: root.requestCurrentTemperature()
    }

    Connections {
        target: Plasmoid.configuration
        function onTemperatureUnitChanged() {
            root.currentTemperature = "";
            root.weatherRequestPending = false;
            Qt.callLater(root.requestCurrentTemperature);
        }
        function onUseOnlineWeatherFallbackChanged() {
            root.currentTemperature = "";
            root.weatherRequestPending = false;
            Qt.callLater(root.requestCurrentTemperature);
        }
    }

    function weatherIcon() {
        return appletIcon("org.kde.plasma.weather", "weather-clear-symbolic");
    }

    Component.onCompleted: {
        activeInstantiator.active = true;
        hiddenInstantiator.active = true;
        Qt.callLater(refreshResponsiveWidth);
    }

    Timer {
        interval: 3000
        running: root.previewMode
        repeat: false
        onTriggered: {
            root.demoNotificationVisible = true;
            Qt.callLater(root.playNotificationIntro);
        }
    }

    // Coalesce notifications that arrive together, then read the settled batch
    // from 1/n onward instead of repeatedly restarting on the same item.
    Timer {
        id: notificationBatchTimer
        interval: 320
        repeat: false
        onTriggered: {
            const row = root.nextUnpresentedNotification();
            if (row < 0) return;
            root.notificationReviewComplete = false;
            root.pendingNotificationCount = 0;
            liveNotificationView.currentIndex = row;
            liveNotificationView.positionViewAtIndex(row, ListView.Contain);
            // Claim the identity before animation starts. Model resets or an
            // interrupted pass must not enqueue the same notification again.
            const presented = Object.assign({}, root.presentedNotificationKeys);
            presented[root.priorityNotificationKey(railNotifications,
                railNotifications.index(row, 0))] = true;
            root.presentedNotificationKeys = presented;
            root.notificationSequenceActive = true;
            root.playNotificationIntro();
        }
    }

    Timer {
        id: nextNotificationIntro
        interval: 260
        repeat: false
        onTriggered: root.playNotificationIntro()
    }

    Connections {
        target: railNotifications
        function onCountChanged() {
            if (!root.notificationsEnabled) {
                root.lastLiveNotificationCount = railNotifications.count;
                root.pendingNotificationCount = 0;
                return;
            }
            if (railNotifications.count > 0) {
                root.notificationReviewComplete = false;
                if (!root.notificationSequenceActive) notificationBatchTimer.restart();
            } else if (railNotifications.count === 0 && !root.demoNotificationVisible) {
                notificationBatchTimer.stop();
                nextNotificationIntro.stop();
                root.notificationSequenceActive = false;
                notificationIntro.stop();
                notificationReveal.stop();
                notificationHide.stop();
                root.notificationCopyActive = false;
                root.pendingNotificationCount = 0;
                root.notificationAutoBatchSize = 0;
                notificationMessageLayer.opacity = 0;
                notificationMessageLayer.x = 2;
            }
            root.lastLiveNotificationCount = railNotifications.count;
        }
    }

    Connections {
        target: Plasmoid
        function onActivated() { root.openSurface("control"); }
    }

    DBus.Properties {
        id: compactPower
        busType: DBus.BusType.System
        service: "org.freedesktop.UPower"
        path: "/org/freedesktop/UPower"
        iface: "org.freedesktop.UPower"
    }

    DBus.Properties {
        id: compactBattery
        busType: DBus.BusType.System
        service: "org.freedesktop.UPower"
        path: "/org/freedesktop/UPower/devices/DisplayDevice"
        iface: "org.freedesktop.UPower.Device"
    }

    NotificationManager.Notifications {
        id: notificationHistory
        showExpired: true
        showDismissed: true
        showAddedDuringInhibition: true
        showJobs: false
        showNotifications: true
        sortMode: NotificationManager.Notifications.SortByDate
        sortOrder: Qt.DescendingOrder
        groupMode: NotificationManager.Notifications.GroupDisabled
        urgencies: NotificationManager.Notifications.LowUrgency
            | NotificationManager.Notifications.NormalUrgency
            | NotificationManager.Notifications.CriticalUrgency
    }

    // The dock rail remains a strict chronological stream. The review window
    // gets a separate view of the same notification store, grouped by app in
    // a sectioned dashboard.
    NotificationManager.Notifications {
        id: groupedNotificationHistory
        showExpired: true
        showDismissed: true
        showAddedDuringInhibition: true
        showJobs: false
        showNotifications: true
        sortMode: NotificationManager.Notifications.SortByDate
        sortOrder: Qt.DescendingOrder
        groupMode: NotificationManager.Notifications.GroupApplicationsFlat
        groupLimit: 1
        expandUnread: false
        urgencies: NotificationManager.Notifications.LowUrgency
            | NotificationManager.Notifications.NormalUrgency
            | NotificationManager.Notifications.CriticalUrgency
    }

    KItemModels.KSortFilterProxyModel {
        id: priorityNotifications
        filterRowCallback: (sourceRow, sourceParent) => {
            if (!root.notificationsEnabled || !Plasmoid.configuration.showPriorityBanners)
                return false;
            const idx = sourceModel.index(sourceRow, 0, sourceParent);
            const expired = Boolean(sourceModel.data(idx,
                NotificationManager.Notifications.ExpiredRole));
            const isFresh = root.isFreshNotification(sourceModel, idx);
            return !expired && isFresh
                && root.isPriorityNotificationCandidate(sourceModel, idx)
                && !root.isPriorityNotificationMinimized(sourceModel, idx);
        }
        Component.onCompleted: sourceModel = notificationHistory
    }

    KItemModels.KSortFilterProxyModel {
        id: railNotifications
        filterRoleName: "read"
        filterRowCallback: (sourceRow, sourceParent) => {
            const idx = sourceModel.index(sourceRow, 0, sourceParent);
            const unread = !sourceModel.data(idx, filterRole)
                || root.isFreshLogoutCancellation(sourceModel, idx);
            const reservedForBanner = root.notificationsEnabled
                && Plasmoid.configuration.showPriorityBanners
                && root.isPriorityNotificationCandidate(sourceModel, idx);
            // A minimized banner remains in history, but has already had its
            // presentation. Do not queue another automatic readout.
            return unread && !reservedForBanner
                && !root.isPriorityNotificationMinimized(sourceModel, idx);
        }
        Component.onCompleted: sourceModel = notificationHistory
    }

    // Logout cancellation entries are short-lived by design. Refresh the two
    // live surfaces so the exception expires even if the history model itself
    // does not emit another change after the event arrives.
    Timer {
        interval: 1000
        repeat: true
        running: priorityNotifications.count > 0
        onTriggered: {
            priorityNotifications.invalidateFilter();
            railNotifications.invalidateFilter();
        }
    }

    KItemModels.KSortFilterProxyModel {
        id: activeModel
        filterRoleName: "effectiveStatus"
        filterRowCallback: (sourceRow, sourceParent) => {
            const idx = sourceModel.index(sourceRow, 0, sourceParent);
            return sourceModel.data(idx, filterRole) === PlasmaCore.Types.ActiveStatus;
        }
        Component.onCompleted: sourceModel = Plasmoid.systemTrayModel
    }

    KItemModels.KSortFilterProxyModel {
        id: hiddenModel
        filterRoleName: "effectiveStatus"
        filterRowCallback: (sourceRow, sourceParent) => {
            const idx = sourceModel.index(sourceRow, 0, sourceParent);
            return sourceModel.data(idx, filterRole) === PlasmaCore.Types.PassiveStatus;
        }
        Component.onCompleted: sourceModel = Plasmoid.systemTrayModel
    }

    KItemModels.KSortFilterProxyModel {
        id: organizedTrayModel
        filterRoleName: "effectiveStatus"
        filterRowCallback: (sourceRow, sourceParent) => {
            const idx = sourceModel.index(sourceRow, 0, sourceParent);
            const state = sourceModel.data(idx, filterRole);
            const itemId = sourceModel.data(idx, Qt.UserRole + 2);
            const frontDoorItems = [
                "org.kde.plasma.volume", "org.kde.plasma.brightness",
                "org.kde.plasma.battery"
            ];
            if (root.notificationsEnabled)
                frontDoorItems.push("org.kde.plasma.notifications");
            if (root.weatherEnabled)
                frontDoorItems.push("org.kde.plasma.weather");
            frontDoorItems.push(...(Plasmoid.configuration.controlCenterItems || []));
            return state !== PlasmaCore.Types.HiddenStatus && frontDoorItems.indexOf(itemId) === -1;
        }
        Component.onCompleted: sourceModel = Plasmoid.systemTrayModel
    }

    KItemModels.KSortFilterProxyModel {
        id: controlCenterModel
        filterRoleName: "itemId"
        sortRoleName: "controlCenterOrder"
        sortOrder: Qt.AscendingOrder
        filterRowCallback: (sourceRow, sourceParent) => {
            const idx = sourceModel.index(sourceRow, 0, sourceParent);
            const itemId = sourceModel.data(idx, filterRole);
            return (Plasmoid.configuration.controlCenterItems || []).indexOf(itemId) !== -1;
        }
        Component.onCompleted: sourceModel = Plasmoid.systemTrayModel
    }

    Connections {
        target: Plasmoid.configuration
        function onControlCenterItemsChanged() {
            controlCenterModel.invalidateFilter();
            organizedTrayModel.invalidateFilter();
        }
        function onShowPriorityBannersChanged() {
            priorityNotifications.invalidateFilter();
        }
        function onShowNotificationsChanged() {
            priorityNotifications.invalidateFilter();
            organizedTrayModel.invalidateFilter();
        }
        function onShowWeatherChanged() {
            organizedTrayModel.invalidateFilter();
        }
        function onPriorityAlertDurationChanged() {
            priorityNotifications.invalidateFilter();
            railNotifications.invalidateFilter();
        }
    }

    KItemModels.KSortFilterProxyModel {
        id: frontDoorModel
        filterRoleName: "hasApplet"
        filterRowCallback: (sourceRow, sourceParent) => {
            const idx = sourceModel.index(sourceRow, 0, sourceParent);
            const hasApplet = sourceModel.data(idx, filterRole);
            const itemId = sourceModel.data(idx, Qt.UserRole + 2);
            return hasApplet && [
                "org.kde.plasma.volume", "org.kde.plasma.brightness", "org.kde.plasma.networkmanagement",
                "org.kde.plasma.battery", "org.kde.plasma.weather"
            ].indexOf(itemId) !== -1;
        }
        Component.onCompleted: sourceModel = Plasmoid.systemTrayModel
    }

    Instantiator {
        id: hiddenInstantiator
        active: false
        model: hiddenModel
        delegate: Connections {
            required property QtObject applet
            required property string itemId
            required property int row
            target: applet
            Component.onCompleted: root.registerApplet(itemId, applet)
            Component.onDestruction: root.unregisterApplet(itemId, applet)
        }
    }

    Instantiator {
        id: activeInstantiator
        active: false
        model: activeModel
        delegate: Connections {
            required property QtObject applet
            required property string itemId
            required property int row
            target: applet
            Component.onCompleted: root.registerApplet(itemId, applet)
            Component.onDestruction: root.unregisterApplet(itemId, applet)
        }
    }

    // Native detail pages need a visual parent even though their stock compact
    // icons are replaced by our combined controls.
    Item {
        visible: false
        width: root.itemSize
        height: root.itemSize
        Repeater {
            model: frontDoorModel
            delegate: ItemLoader {
                width: root.itemSize
                height: root.itemSize
            }
        }
    }

    MouseArea {
        id: compactSurface
        width: root.width
        height: Math.min(42, root.height)
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        onWheel: wheel => wheel.accepted = true

        SystemTrayState { id: systemTrayState }
        DnD.DropArea { anchors.fill: parent; preventStealing: true }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Kirigami.Units.smallSpacing
            anchors.rightMargin: 4
            spacing: 2

            Rectangle {
                id: notificationRail
                Layout.fillWidth: true
                Layout.minimumWidth: 34
                Layout.fillHeight: true
                Layout.topMargin: 4
                Layout.bottomMargin: 4
                radius: height / 2
                color: "transparent"
                RowLayout {
                    anchors.fill: parent
                    spacing: 4
                    layoutDirection: Qt.RightToLeft

                    Rectangle {
                        id: notificationControls
                        visible: root.notificationsEnabled
                        readonly property real expandedWidth: root.hasAttention ? 56 : 26
                        property bool layoutOpen: active || revealed
                        // The notification control is a permanent live icon beside the
                        // status group. Paging expands left into ticker space.
                        Layout.preferredWidth: layoutOpen ? expandedWidth : 26
                        Layout.minimumWidth: Layout.preferredWidth
                        Layout.maximumWidth: Layout.preferredWidth
                        Layout.fillHeight: true
                        property bool revealed: false
                        readonly property bool active: systemTrayState.activeApplet === null
                            && systemTrayState.page === "notifications"
                            && systemTrayState.expanded
                        color: "transparent"
                        clip: true
                        HoverHandler {
                            id: sharedControlsHover
                            onHoveredChanged: notificationControls.syncSharedHover()
                        }
                        function syncSharedHover() {
                            const pointerInside = reviewNotificationsButton.hovered
                                || nextNotificationButton.hovered || sharedControlsHover.hovered;
                            if (pointerInside) {
                                sharedHoverRelease.stop();
                                revealed = true;
                            } else {
                                sharedHoverRelease.restart();
                            }
                        }
                        onRevealedChanged: {
                            if (revealed) {
                                controlsCollapseDelay.stop();
                                layoutOpen = true;
                                if (!root.notificationReviewComplete) root.revealNotificationText();
                            } else {
                                root.hideNotificationText();
                                if (!active) controlsCollapseDelay.restart();
                            }
                        }
                        onActiveChanged: {
                            if (active) {
                                controlsCollapseDelay.stop();
                                layoutOpen = true;
                            } else if (!revealed) {
                                controlsCollapseDelay.restart();
                            }
                        }
                        Timer {
                            id: sharedHoverRelease
                            interval: 280
                            repeat: false
                            onTriggered: {
                                const pointerInside = reviewNotificationsButton.hovered
                                    || nextNotificationButton.hovered || sharedControlsHover.hovered;
                                if (!pointerInside) notificationControls.revealed = false;
                            }
                        }
                        Timer {
                            id: controlsCollapseDelay
                            interval: 130
                            repeat: false
                            onTriggered: {
                                if (!notificationControls.revealed && !notificationControls.active)
                                    notificationControls.layoutOpen = false;
                            }
                        }
                        Connections {
                            target: reviewNotificationsButton
                            function onHoveredChanged() { notificationControls.syncSharedHover(); }
                        }
                        Connections {
                            target: nextNotificationButton
                            function onHoveredChanged() { notificationControls.syncSharedHover(); }
                        }
                        RailChevronButton {
                            id: nextNotificationButton
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            opacity: notificationControls.revealed && root.hasAttention
                                ? (root.notificationReviewComplete ? 0.58 : 1) : 0
                            enabled: notificationControls.revealed && root.hasAttention
                            property bool canPage: railNotifications.count > 0
                                ? liveNotificationView.currentIndex < railNotifications.count - 1
                                : root.demoNotificationIndex < root.demoNotificationTexts.length - 1
                            pointsLeft: true
                            Behavior on opacity { NumberAnimation { duration: 110; easing.type: Easing.InOutCubic } }
                            onClicked: {
                                if (!canPage) {
                                    if (!root.notificationReviewComplete) {
                                        root.notificationReviewComplete = true;
                                        root.hideNotificationText();
                                        return;
                                    }
                                    root.notificationReviewComplete = false;
                                    if (railNotifications.count > 0) {
                                        liveNotificationView.currentIndex = 0;
                                        liveNotificationView.positionViewAtIndex(0, ListView.Beginning);
                                    } else {
                                        root.demoNotificationIndex = 0;
                                    }
                                    root.revealNotificationText();
                                    return;
                                }
                                root.notificationReviewComplete = false;
                                if (railNotifications.count > 0) {
                                    liveNotificationView.currentIndex++;
                                    liveNotificationView.positionViewAtIndex(liveNotificationView.currentIndex, ListView.Contain);
                                } else {
                                    root.demoNotificationIndex++;
                                }
                                root.revealNotificationText();
                            }
                        }

                        RailControlButton {
                            id: reviewNotificationsButton
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            z: 30
                            text: i18n("Review all notifications")
                            onClicked: root.openNotifications()
                        }

                        Item {
                            id: notificationCountBadge
                            anchors.horizontalCenter: reviewNotificationsButton.horizontalCenter
                            y: Math.min(notificationControls.height - height,
                                notificationControls.height / 2 + 6)
                            visible: opacity > 0
                            opacity: root.hasAttention ? 1 : 0
                            scale: root.hasAttention ? 1 : 0.72
                            width: Math.max(18, notificationCountLabel.implicitWidth + 6)
                            height: 12
                            z: 40
                            Behavior on opacity {
                                NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
                            }
                            Behavior on scale {
                                NumberAnimation { duration: 180; easing.type: Easing.OutBack }
                            }

                            PlasmaComponents.Label {
                                id: notificationCountLabel
                                anchors.centerIn: parent
                                text: root.notificationReviewComplete ? "✓"
                                    : notificationControls.revealed
                                    ? (railNotifications.count > 0
                                        ? (liveNotificationView.currentIndex + 1) + "/" + railNotifications.count
                                        : (root.demoNotificationIndex + 1) + "/" + root.demoNotificationTexts.length)
                                    : (railNotifications.count > 0
                                        ? railNotifications.count : root.demoNotificationTexts.length)
                                font.pixelSize: 8
                                font.weight: Font.Medium
                                color: "#F8F8FF"
                            }
                        }
                    }

                    Item {
                        id: notificationContentArea
                        visible: root.notificationsEnabled
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        Item {
                            id: notificationMessageLayer
                            width: parent.width
                            height: parent.height
                            x: 2
                            opacity: 0
                            readonly property real flybyWidth: railNotifications.count > 0 && liveNotificationView.currentItem
                                ? liveNotificationView.currentItem.flybyWidth
                                : demoNotificationTicker.contentWidth

                        ListView {
                            id: liveNotificationView
                            anchors.fill: parent
                            anchors.rightMargin: Kirigami.Units.smallSpacing
                            visible: railNotifications.count > 0
                            interactive: false
                            // The outer content area owns the physical dock
                            // boundary. This inner clip would cut off long text.
                            clip: false
                            orientation: ListView.Horizontal
                            model: railNotifications
                            delegate: RowLayout {
                                id: notificationDelegate
                                visible: ListView.isCurrentItem
                                required property string summary
                                required property string body
                                required property string applicationName
                                required property string applicationIconName
                                width: liveNotificationView.width
                                height: liveNotificationView.height
                                readonly property real flybyWidth: notificationTicker.contentWidth
                                spacing: 0
                                RailTicker {
                                    id: notificationTicker
                                    Layout.fillWidth: true
                                    alignRight: true
                                    exposeOverflow: notificationIntro.running
                                    text: root.notificationDisplayText(notificationDelegate.applicationName,
                                        notificationDelegate.summary, notificationDelegate.body)
                                    scrollingEnabled: notificationControls.revealed
                                        && !notificationReveal.running && !root.notificationReviewComplete
                                }
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.rightMargin: Kirigami.Units.smallSpacing
                            visible: railNotifications.count === 0 && root.demoNotificationVisible
                            spacing: 0
                            RailTicker {
                                id: demoNotificationTicker
                                Layout.fillWidth: true
                                alignRight: true
                                exposeOverflow: notificationIntro.running
                                text: root.demoNotificationTexts[root.demoNotificationIndex]
                                scrollingEnabled: notificationControls.revealed
                                    && !notificationReveal.running && !root.notificationReviewComplete
                            }
                        }
                        }

                        SequentialAnimation {
                            id: notificationIntro
                            ScriptAction {
                                script: {
                                    root.notificationCopyActive = true;
                                    notificationMessageLayer.x = notificationContentArea.width;
                                    notificationMessageLayer.opacity = 0;
                                }
                            }
                            PauseAnimation { duration: 210 }
                            ScriptAction {
                                script: {
                                    notificationMessageLayer.opacity = 1;
                                }
                            }
                            NumberAnimation {
                                target: notificationMessageLayer
                                property: "x"
                                to: -Math.max(notificationMessageLayer.width, notificationMessageLayer.flybyWidth)
                                duration: Math.max(3000,
                                    (notificationContentArea.width + notificationMessageLayer.flybyWidth) * 20)
                                easing.type: Easing.Linear
                            }
                            ScriptAction {
                                script: {
                                    notificationMessageLayer.opacity = 0;
                                    notificationMessageLayer.x = 2;
                                    // Keep weather suppressed between items while a
                                    // collected batch is still reading itself out.
                                    if (!root.notificationSequenceActive) {
                                        root.notificationCopyActive = false;
                                    }
                                }
                            }
                            onFinished: {
                                if (!root.notificationSequenceActive
                                        || notificationControls.revealed
                                        || railNotifications.count === 0) {
                                    root.notificationSequenceActive = false;
                                    root.notificationCopyActive = false;
                                    return;
                                }
                                liveNotificationView.currentIndex = 0;
                                liveNotificationView.positionViewAtIndex(0, ListView.Beginning);
                                root.notificationSequenceActive = false;
                                root.notificationCopyActive = false;
                                root.notificationAutoBatchSize = 0;
                                notificationBatchTimer.restart();
                            }
                        }

                        SequentialAnimation {
                            id: notificationReveal
                            ScriptAction {
                                script: {
                                    root.notificationCopyActive = true;
                                    notificationMessageLayer.x = 2;
                                    notificationMessageLayer.opacity = 0;
                                }
                            }
                            PauseAnimation { duration: 60 }
                            NumberAnimation {
                                target: notificationMessageLayer
                                property: "opacity"
                                to: 1
                                duration: 180
                                easing.type: Easing.OutCubic
                            }
                        }

                        SequentialAnimation {
                            id: notificationHide
                            NumberAnimation {
                                target: notificationMessageLayer
                                property: "opacity"
                                to: 0
                                duration: 130
                                easing.type: Easing.InCubic
                            }
                            ScriptAction {
                                script: {
                                    notificationMessageLayer.x = 2;
                                    root.notificationCopyActive = false;
                                }
                            }
                        }

                        TapHandler {
                            enabled: root.notificationCopyActive
                            onTapped: root.openNotifications()
                        }
                    }
                }

            }

            Item {
                id: weatherStatusButton
                visible: root.weatherEnabled
                Layout.preferredWidth: 26
                Layout.minimumWidth: 26
                Layout.maximumWidth: 26
                Layout.fillHeight: true
                Layout.topMargin: 4
                Layout.bottomMargin: 4
                Accessible.name: i18n("Weather")
                Accessible.role: Accessible.Button

                Item {
                    anchors.fill: parent
                    scale: weatherStatusHover.hovered ? 1.06 : 1
                    Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }

                    Kirigami.Icon {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.horizontalCenterOffset: -1
                        anchors.verticalCenter: parent.verticalCenter
                        width: 16
                        height: 16
                        source: root.weatherIcon()
                        color: "#F8F8FF"
                    }

                    Item {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: Math.min(parent.height - height, parent.height / 2 + 6)
                        width: Math.max(18, weatherTemperatureLabel.implicitWidth + 6)
                        height: 12
                        z: 2

                        PlasmaComponents.Label {
                            id: weatherTemperatureLabel
                            anchors.centerIn: parent
                            text: root.weatherText()
                            font.pixelSize: 8
                            font.weight: Font.Medium
                            color: "#F8F8FF"
                        }
                    }
                }
                HoverHandler { id: weatherStatusHover }
                TapHandler { onTapped: root.activateAppletById("org.kde.plasma.weather") }
                PlasmaComponents.ToolTip { text: i18n("Weather") }
            }

            StatusIconButton {
                statusIcon: root.networkIcon()
                accessibleName: i18n("Control Center")
                active: systemTrayState.activeApplet === null
                    && systemTrayState.page === "control"
                    && systemTrayState.expanded
                onTriggered: root.openSurface("control")
            }

            Item {
                id: batteryStatusButton
                visible: root.hasBattery
                Layout.preferredWidth: root.batteryOnAC ? 26 : Math.max(26, batteryPercentReadout.implicitWidth + 2)
                Layout.minimumWidth: Layout.preferredWidth
                Layout.maximumWidth: Layout.preferredWidth
                Layout.fillHeight: true
                Layout.topMargin: 4
                Layout.bottomMargin: 4
                Accessible.name: i18n("Battery and Control Center")
                Accessible.role: Accessible.Button

                PlasmaComponents.Label {
                    id: batteryPercentReadout
                    visible: !root.batteryOnAC
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.batteryLabel() || "—"
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    color: "#F8F8FF"
                    scale: batteryStatusHover.hovered ? 1.06 : 1
                    Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                }
                Canvas {
                    anchors.centerIn: parent
                    width: 18
                    height: 18
                    visible: root.batteryOnAC
                    onPaint: {
                        const ctx = getContext("2d");
                        ctx.reset();
                        ctx.fillStyle = "#F8F8FF";
                        ctx.beginPath();
                        ctx.moveTo(10.5, 1); ctx.lineTo(3.5, 10);
                        ctx.lineTo(8, 10); ctx.lineTo(7, 17);
                        ctx.lineTo(14.5, 7); ctx.lineTo(10, 7);
                        ctx.closePath(); ctx.fill();
                    }
                }
                HoverHandler { id: batteryStatusHover }
                TapHandler { onTapped: root.openSurface("control") }
                PlasmaComponents.ToolTip {
                    text: root.batteryOnAC ? i18n("AC power · %1", root.batteryLabel())
                        : i18n("Battery and Control Center")
                }
            }

            Item {
                id: trayButton
                Layout.preferredWidth: 26
                Layout.minimumWidth: 26
                Layout.maximumWidth: 26
                Layout.fillHeight: true
                Layout.topMargin: 4
                Layout.bottomMargin: 4
                Accessible.name: i18n("System tray")
                Accessible.role: Accessible.Button
                readonly property bool active: systemTrayState.activeApplet === null
                    && systemTrayState.page === "tray" && systemTrayState.expanded
                property real rippleProgress: 0
                onActiveChanged: {
                    if (active) trayRipple.restart();
                    else { trayRipple.stop(); rippleProgress = 0; }
                }
                NumberAnimation {
                    id: trayRipple
                    target: trayButton
                    property: "rippleProgress"
                    from: 0
                    to: 1
                    duration: 320
                    easing.type: Easing.OutCubic
                }
                Canvas {
                    id: trayGlyph
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 1
                    width: 28
                    height: 28
                    scale: trayHover.hovered ? 1.06 : 1
                    Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                    Connections {
                        target: trayButton
                        function onRippleProgressChanged() { trayGlyph.requestPaint(); }
                        function onActiveChanged() { trayGlyph.requestPaint(); }
                    }
                    onPaint: {
                        const ctx = getContext("2d");
                        ctx.reset();
                        function triangle(size) {
                            const cx = width / 2;
                            const cy = height / 2;
                            const vertices = [[cx, cy - size * 0.55],
                                [cx + size * 0.6, cy + size * 0.4],
                                [cx - size * 0.6, cy + size * 0.4]];
                            // Round the path itself, not just the stroke join.
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
                                ctx.quadraticCurveTo(v[0], v[1],
                                    v[0] + (next[0] - v[0]) * b,
                                    v[1] + (next[1] - v[1]) * b);
                            }
                            ctx.closePath();
                        }
                        ctx.lineJoin = "round";
                        ctx.fillStyle = "#F8F8FF";
                        ctx.strokeStyle = "#F8F8FF";
                        if (trayButton.active) {
                            // Offset the SAME path with a stroke band, rather than
                            // scaling vertices. This keeps clearance uniform around
                            // both the straight edges and the rounded corners.
                            const offset = 2 + trayButton.rippleProgress * 1.6;
                            ctx.globalAlpha = 0.8 - trayButton.rippleProgress * 0.3;
                            ctx.lineWidth = offset * 2 + 1;
                            triangle(9);
                            ctx.stroke();
                            ctx.globalCompositeOperation = "destination-out";
                            ctx.globalAlpha = 1;
                            ctx.lineWidth = offset * 2 - 1;
                            ctx.fill();
                            ctx.stroke();
                            ctx.globalCompositeOperation = "source-over";
                        }
                        ctx.globalAlpha = 1;
                        ctx.lineWidth = 2;
                        triangle(9); ctx.fill(); ctx.stroke();
                    }
                }
                HoverHandler { id: trayHover }
                TapHandler { onTapped: root.openSurface("tray") }
                PlasmaComponents.ToolTip { text: i18n("System tray") }
            }
        }

        Timer {
            id: expandedSync
            interval: 100
            onTriggered: systemTrayState.expanded = dialog.visible
        }

        // Keep every popup attached to the status end of the responsive rail.
        // Plasma handles screen-edge clamping; the dialog's floating margin
        // supplies the deliberate 10 px safety from the panel and screen.
        Item {
            id: popupAnchor
            width: root.vertical ? compactSurface.width : 1
            height: compactSurface.height
            x: root.vertical
                ? (Plasmoid.location === PlasmaCore.Types.LeftEdge ? 10
                    : Plasmoid.location === PlasmaCore.Types.RightEdge ? -10 : 0)
                : compactSurface.width - width
            y: Plasmoid.location === PlasmaCore.Types.TopEdge ? 10
                : Plasmoid.location === PlasmaCore.Types.BottomEdge ? -10 : 0
        }

        Item {
            id: criticalPopupAnchor
            width: root.vertical ? compactSurface.width : 1
            height: compactSurface.height
            x: root.vertical ? popupAnchor.x
                : compactSurface.width - width
            // CriticalNotification windows do not inherit the same panel-side
            // floating clearance as AppletPopup windows.
            y: popupAnchor.y + (Plasmoid.location === PlasmaCore.Types.TopEdge ? 10
                : Plasmoid.location === PlasmaCore.Types.BottomEdge ? -10 : 0)
        }

        PlasmaCore.Dialog {
            id: dialog
            objectName: "popupWindow"
            visualParent: popupAnchor
            location: Plasmoid.location
            type: PlasmaCore.Dialog.AppletPopup
            floating: 10
            hideOnWindowDeactivate: !(Plasmoid.configuration.showPinButton
                && Plasmoid.configuration.pin)
            visible: systemTrayState.expanded
            appletInterface: root
            // Unlike AppletPopup, Dialog genuinely supports a frameless host.
            // ExpandedRepresentation owns the complete rounded #141414 surface.
            backgroundHints: PlasmaCore.Dialog.NoBackground
            onVisibleChanged: {
                if (!visible) expandedSync.restart();
                else requestActivate();
            }
            mainItem: ExpandedRepresentation {
                id: expandedRepresentation
                Keys.onEscapePressed: systemTrayState.expanded = false
                Item { id: preloadedStorage; visible: false }
            }
        }

        PlasmaCore.Dialog {
            id: criticalAlertPopup
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
                readonly property real heightLimit: root.notificationPopupHeightLimit
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
                    width: bannerStack.width
                    implicitHeight: criticalContent.implicitHeight + 32
                    visible: false
                    clip: true
                    property real entranceProgress: 0
                    opacity: entranceProgress
                    transform: Translate {
                        y: (1 - criticalCard.entranceProgress) * 12
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
                        duration: 180
                        easing.type: Easing.OutCubic
                    }
                    onImplicitHeightChanged: Qt.callLater(bannerStack.reflow)
                    Behavior on y {
                        enabled: criticalCard.visible
                        NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
                    }
                    color: "#141414"
                    radius: 18
                    border.width: 1
                    border.color: "#333333"

                    RowLayout {
                        id: criticalContent
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        Kirigami.Icon {
                            Layout.alignment: Qt.AlignTop
                            source: criticalCard.applicationIconName || "dialog-warning-symbolic"
                            implicitWidth: Kirigami.Units.iconSizes.medium
                            implicitHeight: implicitWidth
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            PlasmaComponents.Label {
                                Layout.fillWidth: true
                                text: criticalCard.applicationName || i18n("System alert")
                                opacity: 0.62
                                elide: Text.ElideRight
                            }
                            PlasmaComponents.Label {
                                Layout.fillWidth: true
                                text: criticalCard.summary
                                font.weight: Font.DemiBold
                                wrapMode: Text.Wrap
                            }
                            PlasmaComponents.Label {
                                Layout.fillWidth: true
                                visible: text.length > 0
                                text: criticalCard.body
                                opacity: 0.78
                                wrapMode: Text.Wrap
                                maximumLineCount: 3
                                elide: Text.ElideRight
                            }
                        }

                        PlasmaComponents.ToolButton {
                            icon.name: "window-minimize-symbolic"
                            display: PlasmaComponents.AbstractButton.IconOnly
                            text: i18n("Keep for review")
                            onClicked: root.minimizePriorityNotification(
                                priorityNotifications.mapToSource(
                                    priorityNotifications.index(criticalCard.index, 0)))
                            PlasmaComponents.ToolTip { text: parent.text }
                        }

                        PlasmaComponents.ToolButton {
                            icon.name: "window-close-symbolic"
                            display: PlasmaComponents.AbstractButton.IconOnly
                            text: i18n("Dismiss")
                            onClicked: notificationHistory.close(
                                priorityNotifications.mapToSource(
                                    priorityNotifications.index(criticalCard.index, 0)))
                            PlasmaComponents.ToolTip { text: parent.text }
                        }
                    }

                    TapHandler {
                        onTapped: {
                            const modelIndex = priorityNotifications.mapToSource(
                                priorityNotifications.index(criticalCard.index, 0));
                            if (criticalCard.hasDefaultAction) {
                                notificationHistory.invokeDefaultAction(modelIndex);
                            } else {
                                root.minimizePriorityNotification(modelIndex);
                                root.openSurface("notifications");
                            }
                        }
                    }
                }
                }
            }
        }
    }
}
