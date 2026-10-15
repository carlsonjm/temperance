/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

import QtQuick

// Today's timed events from the linked calendars that have not ended and
// were not dismissed, soonest first; they head the notification history.
// Holidays and all-day events stay in the calendar. The arrow marks one
// seen, which takes it off the ticker and leaves it in the history; Dismiss
// there removes it. Both marks are kept per day, so a restart keeps them.
QtObject {
    id: events

    // The CalendarFeeds of the linked calendars, or null.
    required property var feeds
    required property bool enabled
    // The time now, following Plasma's clock.
    required property date now
    // Where the marks are kept: readEvents and dismissedEvents, as day|key.
    required property var configuration
    // The Status Bar's clock, whose form an event's start is written in.
    required property var clock

    readonly property string day: Qt.formatDate(now, "yyyy-MM-dd") + "|"
    // Yesterday's marks still count for an event that runs past midnight.
    readonly property string previousDay: Qt.formatDate(
        new Date(now.getFullYear(), now.getMonth(), now.getDate() - 1), "yyyy-MM-dd") + "|"
    readonly property var seenKeys: dayKeys(configuration.readEvents)
    readonly property var dismissedKeys: dayKeys(configuration.dismissedEvents)
    readonly property var today: {
        if (!feeds || !enabled)
            return [];
        feeds.revision;
        const day = feeds.eventsForDay(now.getFullYear(), now.getMonth() + 1, now.getDate());
        // An overnight event stays until it ends, past midnight.
        return day.filter(event => (!event.allDay || event.continuing) && event.end > now
            && !dismissedKeys.includes(event.key));
    }
    // The next of those not yet seen rides the ticker.
    readonly property var unseen: today.filter(event => !seenKeys.includes(event.key))
    readonly property var next: unseen.length > 0 ? unseen[0] : null

    function dayKeys(entries) {
        return (entries || []).filter(entry => entry.startsWith(day) || entry.startsWith(previousDay))
            .map(entry => entry.slice(day.length));
    }

    // Adds today's mark for an event to a stored list, dropping older days'.
    function withDayKey(entries, key) {
        const kept = (entries || []).filter(entry => entry.startsWith(day) || entry.startsWith(previousDay));
        if (!kept.includes(day + key))
            kept.push(day + key);
        return kept;
    }

    function markSeen(key) {
        configuration.readEvents = withDayKey(configuration.readEvents, key);
    }

    function dismiss(key) {
        configuration.dismissedEvents = withDayKey(configuration.dismissedEvents, key);
    }

    // What the ticker says of an event's time: that it is under way, how soon
    // it starts within the hour, or its start in the clock's own form. A start
    // time beside the clock read as a second clock, and one already passed
    // said nothing.
    function timeText(event) {
        if (!event) return "";
        if (event.start <= now) return i18nc("@info an event under way", "Now");
        const minutes = Math.ceil((event.start - now) / 60000);
        if (minutes < 60)
            return i18ncp("@info an event starting soon", "In %1 min", "In %1 min", minutes);
        return clock.timeString(event.start);
    }

    // The day period ending a clock-form time, which the ticker draws smaller
    // and lighter as the clock does; empty for any other lead.
    function timePeriod(text) {
        for (const label of [clock.amLabel, clock.pmLabel]) {
            if (label && text.endsWith(" " + label)) return label;
        }
        return "";
    }
}
