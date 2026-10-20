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
import "StatusGlyphs.js" as StatusGlyphs

ContainmentItem {
    id: root

    StatusColors {
        id: tone
        theme: root.Kirigami.Theme
    }

    readonly property bool vertical: Plasmoid.formFactor === PlasmaCore.Types.Vertical
    readonly property int itemSize: Kirigami.Units.iconSizes.smallMedium
    // The row is drawn for a 42 px surface. A horizontal panel too thin to
    // hold that, a bar along the top of the screen say, gets the slim row:
    // the same controls, smaller and on one line, in the height the panel
    // gives. The panel's thickness alone decides; no setting chooses it.
    readonly property real rowHeight: vertical || height <= 0 ? 42 : Math.min(42, height)
    readonly property bool slim: rowHeight < 34
    // The status icons are drawn a third larger than the panel's small icons
    // and set one pitch apart with no gap, so the row reads as one group. The
    // pitch is narrower than a fingertip wants, so each icon also answers a
    // touch anywhere in the panel's height above or below it.
    readonly property int statusPitch: slim ? 28 : 34
    // The drawn outlines, the bell and the wired port, are kept to the size
    // of the theme's filled marks beside them, in a line lighter than 2 px:
    // an outline reads larger than a filled mark of the same box.
    readonly property int bellSize: slim ? 16 : 21
    readonly property real bellStroke: slim ? 1.5 : 1.75
    // The bell sits a little low, keeping one optical baseline whether its
    // clapper shows or its mark replaces it. The slim row has no room for a
    // mark under the bell, so there the bell stays near centre and gives its
    // place to a chip holding the count of what is unread: the lines waiting
    // and those checked off but not yet read in the history. Do not disturb
    // shows the slashed bell instead, whatever waits.
    readonly property real bellDrop: slim ? 1 : bellSize * 0.1
    readonly property int unreadCount: attentionPages + checkedNotifications.count
    // Set to show the bell alone, it keeps its clapper and wears no mark or
    // chip; its name still says the count, to a screen reader.
    readonly property bool bellCounts: Plasmoid.configuration.waitingShows !== "bell"
    readonly property bool bellShowsCount: bellCounts && slim && !doNotDisturb && unreadCount > 0
    readonly property string unreadText: unreadCount > 9
        ? i18nc("@info:status more than nine unread", "9+") : Qt.locale().toString(unreadCount)
    // A theme's status icon, at a size themes draw: 24, or 16 in the slim
    // row. A symbolic one fills its box, so it is asked for smaller.
    readonly property int statusIconSize: slim ? 16 : 24
    readonly property int symbolicIconSize: slim ? 16 : 21
    readonly property int wiredGlyphSize: slim ? 14 : 18
    // The battery's percentage, and the slim row's line of text.
    readonly property int statusTextPixelSize: slim ? 12 : 15
    readonly property font slimFont: Qt.font({ family: Kirigami.Theme.defaultFont.family, pixelSize: 13 })
    readonly property font slimSmallFont: Qt.font({ family: Kirigami.Theme.defaultFont.family, pixelSize: 11 })
    // The status control a touch outside every control's own box is on.
    property Item reachedControl: null
    // When a finger last pressed or lifted anywhere on the panel. Its press and
    // lift also arrive as a pointer, which is not one resting on the bell.
    property double lastTouchAt: 0
    // A finger on the slim row's status controls, which are too small to
    // press one by one. A tap opens the page of the control nearest it, as
    // a click would, a press between two taking the nearer. Pulled down past
    // the threshold, the controls grow out of the bar finger-sized and the
    // finger chooses among them; lifting opens the one under it. The
    // threshold is past a tap's wobble, Qt's 10 px, and two thirds of a
    // 24 px bar, so a press at the bar's top is pulled out of the bar.
    readonly property int statusPullThreshold: 16
    // The gap between the bar and the strip.
    readonly property int magnifierGap: 4
    // None, pressed (a tap so far), or pulled (choosing in the strip).
    property int statusTouchPhase: 0
    property point statusTouchStart: Qt.point(0, 0)
    property real statusPull: 0
    property bool magnifierShown: false
    property real magnifierGrowth: 0
    property real magnifierFingerX: -1
    // Where the press was, in this item, and where the face of the control
    // it pressed sits in the strip. Along an edge of the screen the strip
    // cannot stand with that face under the finger, so the finger's travel to
    // that side is stretched to reach the strip's end before the screen's.
    property real magnifierPressX: 0
    property real magnifierPressCentre: 0
    property real magnifierGainLeft: 1
    property real magnifierGainRight: 1
    property int magnifierSelected: -1
    property var magnifierEntries: []
    readonly property bool magnifierUpward: Plasmoid.location === PlasmaCore.Types.BottomEdge
    readonly property bool oneRowOrColumn: true
    readonly property alias systemTrayState: systemTrayState
    readonly property alias hiddenLayout: expandedRepresentation.hiddenLayout
    readonly property alias hiddenModel: hiddenModel
    readonly property alias organizedTrayModel: organizedTrayModel
    readonly property alias controlCenterModel: controlCenterModel
    readonly property alias notificationHistoryModel: groupedNotificationHistory
    property var appletsById: ({})
    readonly property bool previewMode: Qt.application.name === "plasmawindowed"
    // The color scheme's highlight, so Control Center, the bell and today in
    // the calendar match the rest of the desktop; a color of its own only
    // when the person turned that off.
    readonly property color accentColor: Plasmoid.configuration.accentFromTheme
        ? Kirigami.Theme.highlightColor : (Plasmoid.configuration.accentColor || "#00F2BA")
    readonly property bool motionEnabled: Kirigami.Units.longDuration > 0
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
    // A new line has come in beside the bell and is resting there; until it
    // rests it is arriving, and does not scroll.
    property bool notificationResting: false
    readonly property bool notificationArriving: notificationRail.notificationIntro.running && !notificationResting
    property bool notificationSequenceActive: false
    property bool notificationReviewComplete: false
    // The arrow's run ends on the calendar's event, after any unread
    // notifications; this is true while it rests there.
    property bool eventPage: false
    property var minimizedPriorityNotifications: ({})
    // Built for another Plasma or Qt, the bar shows only that it needs an
    // update, and leaves notifications and the tray to Plasma.
    readonly property bool needsUpdate: Plasmoid.needsUpdate
    readonly property int needsUpdateWidth: needsUpdateLabel.implicitWidth + 2 * Kirigami.Units.largeSpacing
    readonly property int compactWidth: Math.max(320,
        Math.min(600, Number(Plasmoid.configuration.compactWidth) || 400))
    readonly property bool adaptiveWidth: Plasmoid.configuration.adaptiveWidth !== false
    // Only the controls establish a floor. On the right side of the centered
    // dock, the ticker receives the gap from the nearest real panel item on
    // its left to this applet's fixed right edge.
    readonly property int responsiveMinimumWidth: 10
        + statusPitch // network
        + statusPitch // tray
        + (notificationsEnabled ? notificationRail.Layout.minimumWidth : 0)
        + (weatherEnabled ? weatherStatusButton.Layout.preferredWidth : 0)
        + (hasBattery ? batteryStatusButton.Layout.preferredWidth : 0)
        + (clockEnabled ? statusClock.implicitWidth
            + statusClock.Layout.leftMargin + statusClock.Layout.rightMargin : 0)
    property int responsiveMeasuredWidth: responsiveMinimumWidth
    readonly property bool notificationsEnabled: Plasmoid.configuration.showNotifications !== false
    // With the ticker off the bell stays and counts what waits; nothing
    // scrolls beside it.
    readonly property bool tickerEnabled: notificationsEnabled
        && Plasmoid.configuration.showTicker !== false
    readonly property bool doNotDisturb: NotificationManager.Server.inhibited
    readonly property bool weatherEnabled: Plasmoid.configuration.showWeather === true
    readonly property bool timeEnabled: Plasmoid.configuration.showTime !== false
    readonly property bool dateEnabled: Plasmoid.configuration.showDate === true
    readonly property bool clockEnabled: timeEnabled || dateEnabled
    // 0 keeps a banner until it is dismissed; it still has to arrive within
    // the default window to be shown, so old alerts never replay at login.
    readonly property bool priorityAlertsUntilDismissed:
        Number(Plasmoid.configuration.priorityAlertDuration) === 0
    readonly property int priorityAlertFreshnessMs: (priorityAlertsUntilDismissed ? 20 : Math.max(3,
        Math.min(60, Number(Plasmoid.configuration.priorityAlertDuration) || 20))) * 1000
    // Banners shown while they are kept until dismissed, by key.
    property var heldPriorityNotifications: ({})
    // Unset, the unit follows the region: Fahrenheit where the region
    // measures in US units, Celsius everywhere else.
    readonly property string regionTemperatureUnit:
        Qt.locale().measurementSystem === Locale.ImperialUSSystem ? "fahrenheit" : "celsius"
    readonly property string temperatureUnit: {
        const configured = String(Plasmoid.configuration.temperatureUnit || "");
        return ["weather", "fahrenheit", "celsius"].includes(configured)
            ? configured : regionTemperatureUnit;
    }
    readonly property bool hasBattery: Boolean(compactBattery.properties.IsPresent)
    readonly property var demoNotificationTexts: [
        i18n("FINAL SCROLL TEST: This message is intentionally much longer than the notification rail can display at once. Rest on the bell and keep reading while the remaining sentence glides cleanly into view."),
        i18n("System Update: Packages are ready"),
        i18n("Downloads: Transfer completed")
    ]
    // The bell wears a mark below its rim, in place of its clapper: the lines
    // waiting, or ✓ once all are checked off and the history is still unread.
    readonly property bool bellMarked: bellCounts && (hasAttention || checkedNotifications.count > 0)
    readonly property bool hasAttention: notificationsEnabled
        && (railNotifications.count > 0 || demoNotificationVisible || tickerEvent !== null)
    readonly property int notificationPages: railNotifications.count > 0 ? railNotifications.count
        : demoNotificationVisible ? demoNotificationTexts.length : 0
    readonly property int attentionPages: notificationPages + (tickerEvent !== null ? 1 : 0)

    // Today's events from the linked calendars (TodayEvents.qml).
    TodayEvents {
        id: calendarEvents
        feeds: Plasmoid.calendarFeeds
        enabled: root.notificationsEnabled
        now: systemClock.dateTime
        configuration: Plasmoid.configuration
        clock: statusClock
    }
    readonly property var todayEvents: calendarEvents.today
    readonly property var tickerEvent: calendarEvents.next
    // At rest the event holds the ticker whenever a notification is not
    // reading; while the controls are open it shows only as its own page.
    readonly property bool eventShown: tickerEvent !== null
        && (notificationRail.notificationControls.revealed
            ? eventPage && !notificationReviewComplete
            : !notificationCopyActive)

    function markEventSeen(key) { calendarEvents.markSeen(key); }
    function dismissEvent(key) { calendarEvents.dismiss(key); }
    function eventTimeText(event) { return calendarEvents.timeText(event); }
    function eventTimePeriod(text) { return calendarEvents.timePeriod(text); }

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

    function isPriorityNotificationHeld(model, modelIndex) {
        return priorityAlertsUntilDismissed && Boolean(heldPriorityNotifications[
            priorityNotificationKey(model, modelIndex)]);
    }

    // A transfer, and a drive plugged in that nothing mounted, are Ambient's,
    // which keeps each in view for a minute first. The end of a transfer and
    // the drive, filed in their freedesktop categories, are kept in the
    // history and never played on the ticker.
    function isFiledQuietly(model, modelIndex) {
        const category = model.data(modelIndex, NotificationManager.Notifications.CategoryRole);
        return category === "transfer.complete" || category === "transfer.error"
            || category === "device.added";
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

    implicitWidth: needsUpdate ? needsUpdateWidth : adaptiveWidth ? responsiveMeasuredWidth : compactWidth
    implicitHeight: slim ? rowHeight : 42
    Layout.fillWidth: false
    Layout.minimumWidth: needsUpdate ? needsUpdateWidth : adaptiveWidth ? responsiveMinimumWidth : compactWidth
    // Responsive mode follows actual neighboring applet geometry. It has no
    // configured, display-derived, or percentage-based width target.
    Layout.preferredWidth: needsUpdate ? needsUpdateWidth : adaptiveWidth ? responsiveMeasuredWidth : compactWidth
    Layout.maximumWidth: needsUpdate ? needsUpdateWidth : adaptiveWidth ? responsiveMeasuredWidth : compactWidth
    // A thin panel's own thickness, so the row never asks for more than it has.
    Layout.minimumHeight: slim ? rowHeight : 42
    Layout.preferredHeight: slim ? rowHeight : 42
    Layout.maximumHeight: slim ? rowHeight : 42

    // The room below a top bar, or above a bottom one, for the popup. Panels
    // arriving after this one shrink it without moving this one, so it is
    // measured again as a page opens: a page taller than the room is put on
    // the far side of the bar, off the screen.
    function refreshPopupHeightLimit() {
        notificationPopupHeightLimit = Plasmoid.availablePopupHeight(root);
    }

    function refreshResponsiveWidth() {
        refreshPopupHeightLimit();
        if (!adaptiveWidth) return;
        // A host that paints its own clock says how much room it leaves
        // beside it; the row's own inset stays inside that width.
        if (Plasmoid.hostAvailableWidth > 0) {
            const given = Math.max(responsiveMinimumWidth, Plasmoid.hostAvailableWidth);
            if (Math.abs(given - responsiveMeasuredWidth) > 1)
                responsiveMeasuredWidth = given;
            return;
        }
        Plasmoid.watchPanelGeometry(root);
        const measured = Plasmoid.availablePanelWidth(root, responsiveMinimumWidth, 6);
        if (Math.abs(measured - responsiveMeasuredWidth) > 1)
            responsiveMeasuredWidth = measured;
    }

    onAdaptiveWidthChanged: Qt.callLater(refreshResponsiveWidth)
    onResponsiveMinimumWidthChanged: Qt.callLater(refreshResponsiveWidth)
    Connections {
        target: Plasmoid
        function onPanelGeometryChanged() { Qt.callLater(root.refreshResponsiveWidth); }
        function onHostAvailableWidthChanged() { Qt.callLater(root.refreshResponsiveWidth); }
    }
    // The output this window is on, for the host's clock; asked once it is
    // known, and again only when the window moves to another output.
    readonly property string outputName: Screen.name
    onOutputNameChanged: if (!needsUpdate) Plasmoid.watchHostClock(outputName)

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
        showApplet(itemId);
    }

    // Opens an applet's page, leaving it open if it is open already.
    function showApplet(itemId) {
        const applet = appletsById[itemId];
        if (!applet) return;
        refreshPopupHeightLimit();
        // Back returns to the page an applet was opened from. One opened from
        // the bar itself, the battery's say, goes back to Control Center,
        // where its page is otherwise reached.
        if (!systemTrayState.expanded
                || (systemTrayState.page !== "control" && systemTrayState.page !== "tray"))
            systemTrayState.page = "control";
        systemTrayState.setActiveApplet(applet);
    }

    function openSurface(page) {
        if (systemTrayState.expanded && !systemTrayState.activeApplet
                && systemTrayState.page === page) {
            systemTrayState.expanded = false;
            return;
        }
        showSurface(page);
    }

    // Opens a page, leaving it open if it is open already.
    function showSurface(page) {
        refreshPopupHeightLimit();
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

    // The ticker's line for a notification: the application in the secondary
    // white as a lead-in, as an event's time is, then the title, then the body
    // in the secondary white. The colour separates them, so no colon.
    function notificationTickerMarkup(applicationName, summary, body) {
        const escape = value => value.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
        const secondary = value => "<font color=\"#A8FFFFFF\">" + value + "</font>";
        const app = plainNotificationText(applicationName);
        const title = plainNotificationText(summary);
        const detail = plainNotificationText(body);
        let line = escape(title);
        if (detail && detail.toLowerCase() !== title.toLowerCase())
            line += (line ? " " : "") + secondary((line ? "· " : "") + escape(detail));
        return app ? secondary(escape(app)) + (line ? "&nbsp;&nbsp;" + line : "") : line;
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
        notificationRail.notificationIntro.stop();
        notificationRail.notificationReveal.stop();
        notificationRail.notificationHide.stop();
        notificationCopyActive = false;
        notificationRail.notificationMessageLayer.opacity = 0;
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
        notificationRail.notificationIntro.stop();
        notificationRail.notificationReveal.stop();
        notificationRail.notificationHide.stop();
        notificationCopyActive = false;
        notificationRail.notificationMessageLayer.opacity = 0;
    }

    function playNotificationIntro() {
        if (!hasAttention) return;
        notificationRail.notificationReveal.stop();
        notificationRail.notificationHide.stop();
        notificationRail.notificationIntro.restart();
    }

    function revealNotificationText() {
        if (!hasAttention) return;
        if (notificationPages === 0)
            eventPage = true;
        notificationBatchTimer.stop();
        nextNotificationIntro.stop();
        notificationSequenceActive = false;
        notificationRail.notificationIntro.stop();
        notificationRail.notificationHide.stop();
        notificationRail.notificationReveal.restart();
    }

    function hideNotificationText() {
        if (!hasAttention) return;
        notificationRail.notificationIntro.stop();
        notificationRail.notificationReveal.stop();
        notificationRail.notificationHide.restart();
    }

    // A finger has no pointer to rest on the bell, so the ticker answers a
    // sideways flick. The line shown is checked off: a notification leaves the
    // ticker as a timed-out popup does, unread in the history until the history
    // is opened, and today's event is marked seen; the next comes up; with no line showing, the
    // flick brings up the latest first, so nothing is set aside unseen. Lines
    // stay up a while after the finger lifts, as under a resting pointer.
    property string tickerCarry: ""
    property real tickerOffset: 0
    property real tickerVelocity: 0
    property real tickerSampleX: 0
    property double tickerSampleAt: 0
    onTickerOffsetChanged: {
        if (tickerCarry === "notification") notificationRail.notificationMessageLayer.x = 2 + tickerOffset;
        else if (tickerCarry === "event") notificationRail.eventLayer.offset = tickerOffset;
    }
    function beginTickerCarry() {
        tickerAway.stop();
        tickerBack.stop();
        touchRevealRelease.stop();
        tickerVelocity = 0;
        tickerSampleX = 0;
        tickerSampleAt = Date.now();
        tickerCarry = notificationCopyActive && notificationPages > 0 && !eventPage ? "notification"
            : notificationRail.eventLayer.opacity > 0 && tickerEvent !== null ? "event" : "";
        if (tickerCarry === "notification") {
            notificationRail.notificationIntro.stop();
            notificationRail.notificationReveal.stop();
            notificationRail.notificationHide.stop();
            notificationRail.notificationMessageLayer.opacity = 1;
        } else if (tickerCarry === "event") {
            notificationRail.eventFade.stop();
            notificationRail.eventExit.stop();
            notificationRail.eventLayer.opacity = 1;
        }
        tickerOffset = 0;
    }
    function carryTicker(dx) {
        const now = Date.now();
        tickerVelocity = 0.6 * ((dx - tickerSampleX) / Math.max(1, now - tickerSampleAt)) + 0.4 * tickerVelocity;
        tickerSampleX = dx;
        tickerSampleAt = now;
        tickerOffset = dx;
    }
    function endTickerCarry() {
        if (tickerCarry === "") {
            holdTickerForTouch();
            return;
        }
        const flicked = Math.abs(tickerVelocity) > 0.6 && Math.sign(tickerVelocity) === Math.sign(tickerOffset);
        if (tickerOffset !== 0 && (Math.abs(tickerOffset) > notificationRail.notificationContentArea.width * 0.35 || flicked)) {
            tickerAway.to = Math.sign(tickerOffset) * notificationRail.notificationContentArea.width;
            tickerAway.start();
        } else {
            tickerBack.start();
        }
    }
    function setTickerLineAside() {
        const carried = tickerCarry;
        tickerCarry = "";
        tickerOffset = 0;
        if (carried === "event" && tickerEvent !== null) {
            notificationRail.eventLayer.opacity = 0;
            notificationRail.eventLayer.offset = 0;
            markEventSeen(tickerEvent.key);
        } else if (carried === "notification") {
            notificationRail.notificationMessageLayer.opacity = 0;
            notificationRail.notificationMessageLayer.x = 2;
            if (railNotifications.count > 0) {
                const row = Math.min(notificationRail.liveNotificationView.currentIndex, railNotifications.count - 1);
                notificationHistory.expire(railNotifications.mapToSource(railNotifications.index(row, 0)));
                railNotifications.invalidateFilter();
                checkedNotifications.invalidateFilter();
                notificationRail.liveNotificationView.currentIndex = Math.max(0, Math.min(row, railNotifications.count - 1));
            } else if (demoNotificationVisible) {
                if (demoNotificationIndex < demoNotificationTexts.length - 1) demoNotificationIndex++;
                else demoNotificationVisible = false;
            }
        }
        holdTickerForTouch();
        revealNotificationText();
    }
    function holdTickerForTouch() {
        notificationRail.notificationControls.revealed = true;
        touchRevealRelease.restart();
    }
    NumberAnimation {
        id: tickerAway
        target: root
        property: "tickerOffset"
        duration: root.motionEnabled ? 160 : 0
        easing.type: Easing.InCubic
        onFinished: root.setTickerLineAside()
    }
    NumberAnimation {
        id: tickerBack
        target: root
        property: "tickerOffset"
        to: 0
        duration: root.motionEnabled ? 240 : 0
        easing.type: Easing.OutBack
        onFinished: {
            root.tickerCarry = "";
            root.holdTickerForTouch();
        }
    }
    Timer {
        id: touchRevealRelease
        interval: 4000
        onTriggered: {
            if (!notificationRail.notificationControls.pointerResting() && !notificationRail.tickerDrag.active)
                notificationRail.notificationControls.revealed = false;
        }
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

    // The network mark, drawn by Temperance whatever the icon theme
    // (StatusGlyphs.js): Plasma's network widget names the state, and the name
    // picks the drawing.
    function networkIcon() {
        const network = appletsById["org.kde.plasma.networkmanagement"];
        return network && network.Plasmoid && network.Plasmoid.icon
            ? StatusGlyphs.network(network.Plasmoid.icon) : "nm-signal-100-symbolic";
    }

    function appletIcon(itemId, fallback) {
        const applet = appletsById[itemId];
        return applet && applet.Plasmoid && applet.Plasmoid.icon
            ? applet.Plasmoid.icon : fallback;
    }

    // The temperature beside the weather icon (WeatherReading.qml).
    WeatherReading {
        id: weatherReading
        applet: root.appletsById["org.kde.plasma.weather"] ?? null
        active: root.weatherEnabled
        unit: root.temperatureUnit
        onlineFallback: Plasmoid.configuration.useOnlineWeatherFallback === true
    }

    function weatherIcon() {
        return appletIcon("org.kde.plasma.weather", "weather-clear-symbolic");
    }

    // The visible status control whose column holds a scene x, for a touch
    // that lands above or below the controls' own boxes.
    function statusControlAt(sceneX) {
        if (vertical) return null;
        const controls = [notificationRail.reviewNotificationsButton, weatherStatusButton, controlCenterButton,
            trayButton, batteryStatusButton, statusClock];
        for (const control of controls) {
            if (!control.visible) continue;
            const x = control.mapFromItem(null, sceneX, 0).x;
            if (x >= 0 && x < control.width) return control;
        }
        return null;
    }

    // The control nearest a finger's press, a press between two taking the
    // closer: the strip grows out with that one under the finger.
    function statusControlNearest(sceneX) {
        if (vertical) return null;
        const controls = [notificationRail.reviewNotificationsButton, weatherStatusButton, controlCenterButton,
            trayButton, batteryStatusButton, statusClock];
        let nearest = null;
        let best = Infinity;
        for (const control of controls) {
            if (!control.visible) continue;
            // The pixels a control covers run from its left edge up to, not
            // including, its right one, so a press on the edge two controls
            // share is the right one's.
            const x = control.mapFromItem(null, sceneX, 0).x;
            const distance = x < 0 ? -x : x >= control.width ? x - control.width + 1 : 0;
            if (distance < best) {
                best = distance;
                nearest = control;
            }
        }
        return nearest;
    }

    // The control a touch outside the row's own surface reaches. Inside the
    // surface the controls answer for themselves: a pointer's press reaches
    // this handler as well as the control's, and acting on both opened a page
    // and closed it again in one click.
    function reachedControlAt(scenePoint) {
        const local = compactSurface.mapFromItem(null, scenePoint.x, scenePoint.y);
        if (compactSurface.contains(local)) return null;
        return statusControlAt(scenePoint.x);
    }

    // Everything a control's own box and the compact surface do not take:
    // the panel's height above and below the row.
    TapHandler {
        id: statusReach
        enabled: !root.needsUpdate
        onPressedChanged: {
            if (point.device && point.device.type === PointerDevice.TouchScreen)
                root.lastTouchAt = Date.now();
            root.reachedControl = pressed ? root.reachedControlAt(point.scenePressPosition) : null;
        }
        onTapped: eventPoint => {
            const control = root.reachedControlAt(eventPoint.scenePosition);
            if (control) control.activate();
        }
    }

    // Where the slim row's status controls start, from the bell or the first
    // control shown after it.
    readonly property real statusControlsLeft: compactSurface.x + statusRow.x
        + (notificationRail.notificationControls.visible ? notificationRail.x + notificationRail.notificationControls.x
            : weatherStatusButton.visible ? weatherStatusButton.x : controlCenterButton.x)

    // The part of the panel a finger uses for its controls, in the panel's
    // own coordinates: the slim row's run of controls, else the whole panel.
    // A bar holding the panel along the top of the screen keeps this part
    // for the controls and leaves the rest, the ticker's room, to a pull
    // that means the workspaces.
    readonly property rect fingerControlsArea: statusTouch.visible
        ? Qt.rect(statusTouch.x, 0, statusTouch.width, height) : Qt.rect(0, 0, width, height)

    // The slim row's status controls, for a finger. It takes the finger's
    // press, so no control under it also acts on it or shows its tooltip;
    // a mouse, touchpad or pen passes through to the controls themselves.
    // The finger stays with the panel that took its press wherever it goes,
    // so the strip below the bar is driven from here.
    MultiPointTouchArea {
        id: statusTouch
        objectName: "temperance-status-touch"
        visible: root.slim && !root.needsUpdate && !root.vertical
        z: 1
        x: root.statusControlsLeft
        width: Math.max(0, compactSurface.x + compactSurface.width - x)
        height: root.height
        mouseEnabled: false
        minimumTouchPoints: 1
        maximumTouchPoints: 1
        onPressed: touchPoints => root.beginStatusTouch(touchPoints[0])
        onUpdated: touchPoints => root.moveStatusTouch(touchPoints[0])
        onReleased: touchPoints => root.endStatusTouch(touchPoints[0])
        onCanceled: root.cancelStatusTouch()
    }

    // The finger's pull away from the bar, toward the rest of the screen.
    function statusPullAt(sceneY) {
        return (magnifierUpward ? -1 : 1) * (sceneY - statusTouchStart.y);
    }

    function beginStatusTouch(point) {
        lastTouchAt = Date.now();
        magnifierShrink.stop();
        magnifierSettle.stop();
        magnifierShown = false;
        magnifierGrowth = 0;
        magnifierSelected = -1;
        statusTouchStart = Qt.point(point.sceneX, point.sceneY);
        statusPull = 0;
        statusTouchPhase = 1;
    }

    function moveStatusTouch(point) {
        if (statusTouchPhase === 0) return;
        statusPull = statusPullAt(point.sceneY);
        if (statusTouchPhase === 1) {
            if (statusPull < statusPullThreshold) return;
            openMagnifier();
        }
        // Out of the bar the strip grows to its full size at once, however
        // far the finger has gone, so the finger chooses from the start;
        // back in the bar it goes back, chooses nothing, and lifted there
        // cancels.
        const out = statusPull >= statusPullThreshold;
        settleMagnifier(out ? 1 : 0);
        const fingerX = root.mapFromItem(null, point.sceneX, 0).x;
        const travel = fingerX - magnifierPressX;
        magnifierFingerX = magnifierPressCentre
            + travel * (travel < 0 ? magnifierGainLeft : magnifierGainRight);
        magnifierSelected = out ? magnifier.indexAt(magnifierFingerX) : -1;
    }

    function endStatusTouch(point) {
        lastTouchAt = Date.now();
        if (statusTouchPhase === 2) moveStatusTouch(point);
        const phase = statusTouchPhase;
        statusTouchPhase = 0;
        reachedControl = null;
        if (phase === 1) {
            const slop = Qt.styleHints.startDragDistance;
            if (Math.abs(point.sceneX - statusTouchStart.x) <= slop
                    && Math.abs(point.sceneY - statusTouchStart.y) <= slop) {
                const tapped = statusControlNearest(statusTouchStart.x);
                if (tapped) tapped.activate();
                else openSurface("control");
            }
        } else if (phase === 2) {
            const chosen = magnifierSelected >= 0 ? magnifierEntries[magnifierSelected] : null;
            closeMagnifier();
            if (chosen) showStatusPage(chosen.key);
        }
    }

    function cancelStatusTouch() {
        const phase = statusTouchPhase;
        statusTouchPhase = 0;
        reachedControl = null;
        if (phase === 2) closeMagnifier();
    }

    // Opens a control's page from the strip, leaving it open if it is open
    // already: the finger chose it, so it does not close it.
    function showStatusPage(key) {
        if (key === "notifications") {
            notificationHistory.lastRead = new Date();
            showSurface("notifications");
        } else if (key === "weather") {
            showApplet("org.kde.plasma.weather");
        } else if (key === "battery" && appletsById["org.kde.plasma.battery"]) {
            showApplet("org.kde.plasma.battery");
        } else {
            showSurface(key === "battery" ? "control" : key);
        }
    }

    // The controls shown in the bar, left to right, and where each sits in
    // the strip's coordinates; the strip is placed so the control the finger
    // pressed grows out under it.
    function openMagnifier() {
        const shown = [
            [notificationRail.notificationControls.visible, "notifications", notificationRail.reviewNotificationsButton,
                notificationRail.reviewNotificationsButton.text],
            [weatherStatusButton.visible, "weather", weatherStatusButton, i18n("Weather")],
            [controlCenterButton.visible, "control", controlCenterButton, i18n("Control Center")],
            [trayButton.visible, "tray", trayButton, i18n("System Tray")],
            [batteryStatusButton.visible, "battery", batteryStatusButton, i18n("Battery")],
            [statusClock.visible, "calendar", statusClock, i18nc("@label the clock's page", "Calendar")],
        ].filter(entry => entry[0]);
        const entries = shown.map(entry => ({ key: entry[1], control: entry[2], name: entry[3],
            barX: 0, barY: 0 }));
        magnifierEntries = entries;
        const stripWidth = magnifier.width;
        const pressed = statusControlNearest(statusTouchStart.x);
        const pressedIndex = entries.findIndex(entry => entry.control === pressed);
        const pressX = root.mapFromItem(null, statusTouchStart.x, 0).x;
        let left = pressedIndex >= 0 ? pressX - magnifier.restCentres[pressedIndex]
            : statusControlsLeft + (compactSurface.x + compactSurface.width - statusControlsLeft) / 2
                - stripWidth / 2;
        // Kept on the screen, as the popup is.
        const onScreen = root.mapToGlobal(0, 0).x - Screen.virtualX;
        left = Math.max(10 - onScreen, Math.min(Screen.width - 10 - stripWidth - onScreen, left));
        magnifierAnchor.x = Math.round(left);
        // The face of the control pressed is chosen first, wherever the
        // strip stands, and each end of the strip is reached a little short
        // of the screen's edge.
        magnifierPressX = pressX;
        magnifierPressCentre = pressedIndex >= 0 ? magnifier.restCentres[pressedIndex]
            : pressX - magnifierAnchor.x;
        const reach = 12;
        const centres = magnifier.restCentres;
        const roomLeft = pressX + onScreen - reach;
        const roomRight = Screen.width - onScreen - pressX - reach;
        const needLeft = centres.length ? magnifierPressCentre - centres[0] : 0;
        const needRight = centres.length ? centres[centres.length - 1] - magnifierPressCentre : 0;
        magnifierGainLeft = needLeft > 0 && roomLeft > 0 ? Math.max(1, needLeft / roomLeft) : 1;
        magnifierGainRight = needRight > 0 && roomRight > 0 ? Math.max(1, needRight / roomRight) : 1;
        const top = magnifierUpward ? -magnifierGap - magnifier.height : root.height + magnifierGap;
        magnifierEntries = entries.map(entry => {
            const centre = entry.control.mapToItem(root, entry.control.width / 2, entry.control.height / 2);
            return Object.assign({}, entry, { barX: centre.x - magnifierAnchor.x, barY: centre.y - top });
        });
        magnifierGrowth = 0;
        magnifierShown = true;
        statusTouchPhase = 2;
    }

    // The strip grows or goes back while the finger is still down, quickly.
    function settleMagnifier(to) {
        if (magnifierSettle.running ? magnifierSettle.to === to : magnifierGrowth === to) return;
        magnifierSettle.stop();
        if (!motionEnabled) {
            magnifierGrowth = to;
            return;
        }
        magnifierSettle.from = magnifierGrowth;
        magnifierSettle.to = to;
        magnifierSettle.start();
    }

    NumberAnimation {
        id: magnifierSettle
        target: root
        property: "magnifierGrowth"
        duration: Kirigami.Units.shortDuration
        easing.type: Easing.OutCubic
    }

    // The strip goes back into the bar at Plasma's animation speed.
    function closeMagnifier() {
        magnifierSettle.stop();
        magnifierShrink.from = magnifierGrowth;
        magnifierShrink.restart();
    }

    NumberAnimation {
        id: magnifierShrink
        target: root
        property: "magnifierGrowth"
        to: 0
        duration: Kirigami.Units.longDuration
        easing.type: Easing.InCubic
        onFinished: {
            root.magnifierShown = false;
            root.magnifierSelected = -1;
        }
    }

    // Where the strip is put: under the bar, or above one along the bottom,
    // as wide as the strip so the window's left edge is the anchor's.
    Item {
        id: magnifierAnchor
        objectName: "temperance-magnifier-anchor"
        width: magnifier.width
        height: 1
        y: root.magnifierUpward ? -root.magnifierGap : root.height + root.magnifierGap - 1
    }

    PlasmaCore.Dialog {
        id: magnifierWindow
        objectName: "temperance-magnifier-window"
        visualParent: magnifierAnchor
        location: Plasmoid.location
        type: PlasmaCore.Dialog.Tooltip
        // It only draws: the finger's touch stays with the panel.
        outputOnly: true
        flags: Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus
        hideOnWindowDeactivate: false
        backgroundHints: PlasmaCore.Dialog.NoBackground
        visible: root.magnifierShown
        mainItem: StatusMagnifier {
            id: magnifier
            status: root
            entries: root.magnifierEntries
            growth: root.magnifierGrowth
            selected: root.magnifierSelected
            upward: root.magnifierUpward
        }
    }

    Component.onCompleted: {
        appletInstantiator.active = true;
        if (!needsUpdate) Plasmoid.watchHostClock(outputName);
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
            notificationRail.liveNotificationView.currentIndex = row;
            notificationRail.liveNotificationView.positionViewAtIndex(row, ListView.Contain);
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
                // A new notification takes the ticker from the event.
                if (railNotifications.count > root.lastLiveNotificationCount) root.eventPage = false;
                if (!root.notificationSequenceActive) notificationBatchTimer.restart();
            } else if (railNotifications.count === 0 && !root.demoNotificationVisible) {
                notificationBatchTimer.stop();
                nextNotificationIntro.stop();
                root.notificationSequenceActive = false;
                notificationRail.notificationIntro.stop();
                notificationRail.notificationReveal.stop();
                notificationRail.notificationHide.stop();
                root.notificationCopyActive = false;
                root.pendingNotificationCount = 0;
                root.notificationAutoBatchSize = 0;
                notificationRail.notificationMessageLayer.opacity = 0;
                notificationRail.notificationMessageLayer.x = 2;
            }
            root.lastLiveNotificationCount = railNotifications.count;
        }
    }

    Connections {
        target: Plasmoid
        enabled: !root.needsUpdate
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

    // Plasma's own clock source: it ticks on the minute and follows time zone
    // changes and resume from suspend, which a local timer would not.
    PlasmaClock.Clock { id: systemClock }

    // The calendars linked in the settings; a link is read when it is added.
    Binding {
        target: Plasmoid.calendarFeeds
        property: "links"
        value: Plasmoid.configuration.calendarLinks
    }
    Binding {
        target: Plasmoid.calendarFeeds
        property: "colors"
        value: {
            const colors = {};
            for (const entry of Plasmoid.configuration.calendarColors || []) {
                const split = entry.indexOf("|");
                if (split > 0)
                    colors[entry.slice(split + 1)] = entry.slice(0, split);
            }
            return colors;
        }
    }

    NotificationManager.Notifications {
        id: notificationHistory
        objectName: "temperance-notification-history"
        showExpired: true
        showDismissed: true
        showAddedDuringInhibition: true
        showJobs: false
        showNotifications: !root.needsUpdate
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
        showNotifications: !root.needsUpdate
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
            if (expired || root.isPriorityNotificationMinimized(sourceModel, idx))
                return false;
            if (root.isPriorityNotificationHeld(sourceModel, idx))
                return true;
            const shown = root.isFreshNotification(sourceModel, idx)
                && root.isPriorityNotificationCandidate(sourceModel, idx);
            if (shown && root.priorityAlertsUntilDismissed)
                root.heldPriorityNotifications[root.priorityNotificationKey(sourceModel, idx)] = true;
            return shown;
        }
        Component.onCompleted: sourceModel = notificationHistory
    }

    KItemModels.KSortFilterProxyModel {
        id: railNotifications
        objectName: "temperance-rail-notifications"
        filterRoleName: "read"
        filterRowCallback: (sourceRow, sourceParent) => {
            const idx = sourceModel.index(sourceRow, 0, sourceParent);
            // Unread as the history counts it, so opening the history takes
            // its lines off the ticker.
            const unread = root.unreadInHistory(sourceModel, idx)
                || root.isFreshLogoutCancellation(sourceModel, idx);
            const reservedForBanner = root.notificationsEnabled
                && Plasmoid.configuration.showPriorityBanners
                && (root.isPriorityNotificationCandidate(sourceModel, idx)
                    || root.isPriorityNotificationHeld(sourceModel, idx));
            // A minimized banner remains in history, but has already had its
            // presentation. Do not queue another automatic readout.
            // Do not disturb stops the ticker as it stops Plasma's popups, and
            // what arrived meanwhile waits in the history instead of playing
            // once it ends.
            return unread && !reservedForBanner
                && !root.doNotDisturb
                && !root.addedDuringDoNotDisturb(sourceModel, idx)
                && !root.isPriorityNotificationMinimized(sourceModel, idx)
                && !root.isFiledQuietly(sourceModel, idx)
                && !sourceModel.data(idx, NotificationManager.Notifications.ExpiredRole);
        }
        Component.onCompleted: sourceModel = notificationHistory
    }

    // Lines checked off the ticker that the history has not shown yet: the bell
    // keeps a ✓ for them until the history is opened. A transfer's end, filed
    // without crossing the ticker, was never checked and raises none. What
    // arrived during do not disturb skipped the ticker, and the ✓ keeps it in
    // view.
    KItemModels.KSortFilterProxyModel {
        id: checkedNotifications
        objectName: "temperance-checked-notifications"
        filterRowCallback: (sourceRow, sourceParent) => {
            const idx = sourceModel.index(sourceRow, 0, sourceParent);
            return root.unreadInHistory(sourceModel, idx)
                && (Boolean(sourceModel.data(idx, NotificationManager.Notifications.ExpiredRole))
                    || root.addedDuringDoNotDisturb(sourceModel, idx))
                && !root.isFiledQuietly(sourceModel, idx);
        }
        Component.onCompleted: sourceModel = notificationHistory
    }
    Connections {
        target: notificationHistory
        function onLastReadChanged() {
            railNotifications.invalidateFilter();
            checkedNotifications.invalidateFilter();
        }
    }
    onDoNotDisturbChanged: {
        railNotifications.invalidateFilter();
        checkedNotifications.invalidateFilter();
        if (doNotDisturb) {
            doNotDisturbSince = new Date();
        } else if (doNotDisturbSince && Plasmoid.coreFocus) {
            Plasmoid.coreFocus.ended(addedSince(doNotDisturbSince));
            doNotDisturbSince = null;
        }
    }

    // When Do Not Disturb went on, to count what arrived while it lasted.
    property var doNotDisturbSince: doNotDisturb ? new Date() : null

    // Notifications the server marked as added during Do Not Disturb since
    // that moment.
    function addedSince(since) {
        let added = 0;
        for (let row = 0; row < notificationHistory.rowCount(); ++row) {
            const modelIndex = notificationHistory.index(row, 0);
            if (!addedDuringDoNotDisturb(notificationHistory, modelIndex)) continue;
            const created = notificationHistory.data(modelIndex, NotificationManager.Notifications.CreatedRole);
            if (created && created.getTime && created.getTime() >= since.getTime()) ++added;
        }
        return added;
    }

    // Split Rock's Core asked to see what came in during Do Not Disturb.
    Connections {
        target: Plasmoid.coreFocus
        enabled: !root.needsUpdate
        function onOpenRequested() { root.showStatusPage("notifications"); }
    }

    // The server marks what arrives while do not disturb is on.
    function addedDuringDoNotDisturb(model, modelIndex) {
        return Boolean(model.data(modelIndex,
            NotificationManager.Notifications.WasAddedDuringInhibitionRole));
    }

    // Unread as the history counts it: not marked read, and newer than the
    // last time the history was opened.
    function unreadInHistory(model, modelIndex) {
        if (model.data(modelIndex, NotificationManager.Notifications.ReadRole)) return false;
        const occurred = model.data(modelIndex, NotificationManager.Notifications.UpdatedRole)
            || model.data(modelIndex, NotificationManager.Notifications.CreatedRole);
        const lastRead = notificationHistory.lastRead;
        const readUntil = lastRead && lastRead.getTime ? lastRead.getTime() : NaN;
        const occurredMs = occurred && occurred.getTime ? occurred.getTime() : NaN;
        return !Number.isFinite(readUntil) || !Number.isFinite(occurredMs) || occurredMs > readUntil;
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

    // While the popup is open the tray holds still: an icon that changes
    // status keeps its place under the finger, and the lists catch up when
    // the popup closes. Arrivals and departures still show at once.
    readonly property bool trayHeld: systemTrayState.expanded
    onTrayHeldChanged: {
        if (trayHeld) return;
        hiddenModel.invalidateFilter();
        organizedTrayModel.invalidateFilter();
    }

    KItemModels.KSortFilterProxyModel {
        id: hiddenModel
        dynamicSortFilter: !root.trayHeld
        filterRoleName: "effectiveStatus"
        filterRowCallback: (sourceRow, sourceParent) => {
            const idx = sourceModel.index(sourceRow, 0, sourceParent);
            return sourceModel.data(idx, filterRole) === PlasmaCore.Types.PassiveStatus;
        }
        Component.onCompleted: sourceModel = Plasmoid.systemTrayModel
    }

    KItemModels.KSortFilterProxyModel {
        id: organizedTrayModel
        dynamicSortFilter: !root.trayHeld
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
            // An entry whose applet is not loaded, such as KDE Connect while
            // its service is stopped, leaves no empty pill.
            return sourceModel.data(idx, Qt.UserRole + 3) === true
                && (Plasmoid.configuration.controlCenterItems || []).indexOf(itemId) !== -1;
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

    // Applets by id, for the controls that open their pages. One delegate per
    // tray row, not one per shown or hidden list: a row moving between lists
    // destroys its old delegate later than the new one registers, and that
    // late destruction would take the applet back out.
    Instantiator {
        id: appletInstantiator
        active: false
        model: Plasmoid.systemTrayModel
        delegate: QtObject {
            required property QtObject applet
            required property string itemId
            property QtObject registered: null
            function sync() {
                if (registered && registered !== applet)
                    root.unregisterApplet(itemId, registered);
                registered = applet;
                if (applet) root.registerApplet(itemId, applet);
            }
            onAppletChanged: sync()
            Component.onCompleted: sync()
            Component.onDestruction: {
                if (registered) root.unregisterApplet(itemId, registered);
            }
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

    PlasmaComponents.Label {
        id: needsUpdateLabel
        objectName: "temperance-needs-update"
        visible: root.needsUpdate
        anchors.centerIn: parent
        text: i18nc("@info:status %1 is the widget's name", "%1 needs an update", Plasmoid.title)
    }

    MouseArea {
        id: compactSurface
        objectName: "temperance-compact-surface"
        visible: !root.needsUpdate
        width: root.width
        height: Math.min(42, root.height)
        anchors.right: parent.right
        // Held on the region's middle line rather than on its lower edge. Where
        // the host is taller than this surface, bottom alignment leaves the
        // contents sitting low in the region instead of centred in it.
        anchors.verticalCenter: parent.verticalCenter
        onWheel: wheel => wheel.accepted = true

        SystemTrayState { id: systemTrayState }
        DnD.DropArea { anchors.fill: parent; preventStealing: true }

        RowLayout {
            id: statusRow
            anchors.fill: parent
            anchors.leftMargin: Kirigami.Units.smallSpacing
            anchors.rightMargin: 4
            // The status controls touch, so every point along the row is
            // one control's.
            spacing: 0

            NotificationRail {
                id: notificationRail
                bar: root
                tone: tone
                notificationBatchTimer: notificationBatchTimer
                railNotifications: railNotifications
                tickerAway: tickerAway
            }

            Item {
                id: weatherStatusButton
                visible: root.weatherEnabled
                // The slim row has no room under the icon, so the temperature
                // follows it on the line, and the control widens to hold both.
                Layout.preferredWidth: root.slim
                    ? Math.max(root.statusPitch, Math.ceil(root.statusPitch / 2 + root.statusIconSize / 2
                        + 3 + weatherTemperatureLabel.implicitWidth + 5))
                    : root.statusPitch
                Layout.minimumWidth: Layout.preferredWidth
                Layout.maximumWidth: Layout.preferredWidth
                Layout.fillHeight: true
                Accessible.name: i18n("Weather")
                Accessible.role: Accessible.Button
                function activate() { root.activateAppletById("org.kde.plasma.weather"); }

                StatusFace {
                    anchors.fill: parent
                    touched: weatherStatusTap.pressed || root.reachedControl === weatherStatusButton
                    hovered: weatherStatusHover.hovered

                    Kirigami.Icon {
                        id: weatherIcon
                        anchors.horizontalCenter: parent.horizontalCenter
                        // In the slim row, centred in the control's first pitch.
                        anchors.horizontalCenterOffset: root.slim
                            ? (root.statusPitch - parent.width) / 2 : -1
                        anchors.verticalCenter: parent.verticalCenter
                        width: root.slim ? root.statusIconSize : 21
                        height: width
                        // Kirigami would round 21 down to 16.
                        roundToIconSize: false
                        source: root.weatherIcon()
                        color: tone.text
                    }

                    // Below the icon, and inside the panel where the panel is
                    // too thin to hold it lower.
                    Item {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.horizontalCenterOffset: root.slim
                            ? root.statusPitch / 2 + weatherIcon.width / 2 + 3 + width / 2 - parent.width / 2 : 0
                        y: root.slim ? (parent.height - height) / 2
                            : Math.min(parent.height / 2 + 8, (parent.height + root.height) / 2 - height)
                        width: root.slim ? weatherTemperatureLabel.implicitWidth
                            : Math.max(22, weatherTemperatureLabel.implicitWidth + 8)
                        height: root.slim ? weatherTemperatureLabel.implicitHeight : 15
                        z: 2

                        PlasmaComponents.Label {
                            id: weatherTemperatureLabel
                            anchors.centerIn: parent
                            text: weatherReading.text
                            font.pixelSize: root.slim ? root.statusTextPixelSize : 10
                            font.weight: Font.Medium
                            color: tone.text
                        }
                    }
                }
                HoverHandler { id: weatherStatusHover }
                TapHandler { id: weatherStatusTap; onTapped: weatherStatusButton.activate() }
                PlasmaComponents.ToolTip { text: i18n("Weather") }
            }

            LevelReading {
                id: levelReading
                objectName: "temperance-level-reading"
                audio: Plasmoid.audio
                watching: Plasmoid.configuration.showLevels === true
                quiet: systemTrayState.expanded && systemTrayState.page === "control"
                ink: tone.text
                track: tone.wash(0.24)
                pitch: root.statusPitch
                iconSize: root.statusIconSize
                motionEnabled: root.motionEnabled
                Layout.preferredWidth: room
                Layout.minimumWidth: Layout.preferredWidth
                Layout.maximumWidth: Layout.preferredWidth
                Layout.fillHeight: true
            }

            StatusIconButton {
                id: controlCenterButton
                bar: root
                tone: tone
                objectName: "temperance-control-center"
                statusIcon: root.networkIcon()
                accessibleName: i18n("Control Center")
                active: systemTrayState.activeApplet === null
                    && systemTrayState.page === "control"
                    && systemTrayState.expanded
                onTriggered: root.openSurface("control")
            }

            Item {
                id: trayButton
                objectName: "temperance-tray"
                Layout.preferredWidth: root.statusPitch
                Layout.minimumWidth: root.statusPitch
                Layout.maximumWidth: root.statusPitch
                Layout.fillHeight: true
                Accessible.name: i18n("System Tray")
                Accessible.role: Accessible.Button
                function activate() { root.openSurface("tray"); }
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
                    duration: root.motionEnabled ? 320 : 0
                    easing.type: Easing.OutCubic
                }
                StatusFace {
                    anchors.fill: parent
                    touched: trayTap.pressed || root.reachedControl === trayButton
                    hovered: trayHover.hovered
                    TrayGlyph {
                        id: trayGlyph
                        mark: root.slim ? 9 : 12
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: 1
                        ink: tone.text
                        ring: trayButton.active
                        rippleProgress: trayButton.rippleProgress
                    }
                }
                HoverHandler { id: trayHover }
                TapHandler { id: trayTap; onTapped: trayButton.activate() }
                PlasmaComponents.ToolTip { text: i18n("System Tray") }
            }

            Item {
                id: batteryStatusButton
                visible: root.hasBattery
                Layout.preferredWidth: root.batteryOnAC ? root.statusPitch
                    : Math.max(root.statusPitch, batteryPercentReadout.implicitWidth + 4)
                Layout.minimumWidth: Layout.preferredWidth
                Layout.maximumWidth: Layout.preferredWidth
                Layout.fillHeight: true
                Accessible.name: i18n("Battery")
                Accessible.role: Accessible.Button
                // Plasma's Power and Battery page, as Control Center's battery
                // row opens it; Control Center itself, where that page is missing.
                function activate() {
                    if (root.appletsById["org.kde.plasma.battery"])
                        root.activateAppletById("org.kde.plasma.battery");
                    else
                        root.openSurface("control");
                }

                StatusFace {
                    anchors.fill: parent
                    touched: batteryStatusTap.pressed || root.reachedControl === batteryStatusButton
                    hovered: batteryStatusHover.hovered

                    PlasmaComponents.Label {
                        id: batteryPercentReadout
                        visible: !root.batteryOnAC
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.batteryLabel() || "—"
                        font.pixelSize: root.statusTextPixelSize
                        // The clock's weight: a status is not louder than the time.
                        font.weight: Font.Normal
                        color: tone.text
                    }
                    BoltGlyph {
                        anchors.centerIn: parent
                        width: root.statusIconSize
                        height: root.statusIconSize
                        visible: root.batteryOnAC
                        ink: tone.text
                    }
                }
                HoverHandler { id: batteryStatusHover }
                TapHandler { id: batteryStatusTap; onTapped: batteryStatusButton.activate() }
                PlasmaComponents.ToolTip {
                    text: root.batteryOnAC ? i18n("AC power · %1", root.batteryLabel())
                        : i18n("Battery and Control Center")
                }
            }

            StatusClock {
                id: statusClock
                objectName: "temperance-clock"
                visible: root.clockEnabled
                Layout.leftMargin: 6
                Layout.rightMargin: 6
                Layout.preferredWidth: implicitWidth
                Layout.minimumWidth: implicitWidth
                Layout.maximumWidth: implicitWidth
                Layout.fillHeight: true
                dateTime: systemClock.dateTime
                showTime: root.timeEnabled
                showDate: root.dateEnabled
                timePixelSize: root.slim ? 13 : 15
                fontFamily: Plasmoid.configuration.clockFontFamily || ""
                Accessible.role: Accessible.Button
                Accessible.onPressAction: activate()
                function activate() { root.openSurface("calendar"); }

                TapHandler { onTapped: statusClock.activate() }
            }
        }

        Timer {
            id: expandedSync
            interval: 100
            onTriggered: systemTrayState.expanded = dialog.visible
        }

        // Move the visual parent itself away from the panel. Dialog.floating
        // controls screen-border treatment and does not create this panel gap.
        PanelPopupAnchor {
            id: popupAnchor
            objectName: "popupAnchor"
            vertical: root.vertical
            surfaceWidth: compactSurface.width
            surfaceHeight: compactSurface.height
            panelLocation: Plasmoid.location
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
                && Plasmoid.configuration.pin) && !systemTrayState.holdingOpen
            visible: systemTrayState.expanded
            appletInterface: root
            // Unlike AppletPopup, Dialog genuinely supports a frameless host.
            // ExpandedRepresentation owns the complete rounded #141414 surface.
            backgroundHints: PlasmaCore.Dialog.NoBackground
            onVisibleChanged: {
                if (!visible) {
                    expandedSync.restart();
                } else {
                    root.refreshPopupHeightLimit();
                    requestActivate();
                }
            }
            mainItem: ExpandedRepresentation {
                id: expandedRepresentation
                Keys.onEscapePressed: systemTrayState.expanded = false
                Item { id: preloadedStorage; visible: false }
            }
        }

        // Held open through a switch, the popup takes the keys back once the
        // hold ends, so a tap elsewhere closes it again.
        Connections {
            target: systemTrayState
            function onHoldingOpenChanged() {
                if (!systemTrayState.holdingOpen && dialog.visible) dialog.requestActivate();
            }
        }

        AlertBanners {
            id: criticalAlertPopup
            bar: root
            tone: tone
            criticalPopupAnchor: criticalPopupAnchor
            notificationHistory: notificationHistory
            priorityNotifications: priorityNotifications
        }
    }
}
