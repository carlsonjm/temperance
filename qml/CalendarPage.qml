/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.workspace.calendar as PlasmaCalendar

// The month the clock opens: a grid of days, today in the accent, and the
// selected day's holidays and events beneath. Plasma's calendar backend
// supplies the holidays and the linked calendars the events; the grid itself is
// drawn here so it follows the suite's spacing.
//
// Geometry: the popup insets its pages by 4 px and titles by 16 px. The page
// adds 8 px and each cell insets its circle by 4 px, so the outer day circles,
// the title and the footer all sit on one 16 px margin line. Circles are 8 px
// apart in both directions.
Item {
    id: page

    required property date today
    property color accentColor: Kirigami.Theme.highlightColor
    // The linked calendars (CalendarFeeds), or null where there are none.
    property var feeds: null
    // How an event's start is written; the clock passes its own.
    property var formatTime: value => value.toLocaleTimeString(Qt.locale(), Locale.ShortFormat)

    property date shownMonth: new Date(today.getFullYear(), today.getMonth(), 1)
    property date selectedDate: today
    // Bumped whenever the backend delivers events, so bindings that ask for a
    // day's events ask again.
    property int revision: 0

    readonly property int sideInset: 8
    readonly property int textInset: sideInset + (cellSize - markSize) / 2
    readonly property int cellSize: 42
    readonly property int markSize: 34
    readonly property int topSpacing: 12
    readonly property int bottomSpacing: 24

    readonly property bool showingCurrentMonth: shownMonth.getFullYear() === today.getFullYear()
        && shownMonth.getMonth() === today.getMonth()
    readonly property string title: {
        const month = Qt.locale().standaloneMonthName(shownMonth.getMonth(), Locale.LongFormat);
        return shownMonth.getFullYear() === today.getFullYear()
            ? month : month + " " + shownMonth.getFullYear();
    }

    readonly property int firstWeekday: Qt.locale().firstDayOfWeek % 7
    // One entry per cell, week by week: the day of the month, or 0 for a cell
    // that belongs to a neighbouring month and is left empty.
    readonly property var cells: {
        const year = shownMonth.getFullYear();
        const month = shownMonth.getMonth();
        const lead = (new Date(year, month, 1).getDay() - firstWeekday + 7) % 7;
        const length = new Date(year, month + 1, 0).getDate();
        const count = Math.ceil((lead + length) / 7) * 7;
        const result = [];
        for (let index = 0; index < count; ++index) {
            const day = index - lead + 1;
            result.push(day >= 1 && day <= length ? day : 0);
        }
        return result;
    }

    readonly property bool hasLinks: feeds !== null && feeds.links.length > 0
    // The linked calendars' events for the shown month, keyed by day.
    readonly property var linkedEvents: {
        if (!feeds)
            return {};
        feeds.revision;
        return feeds.eventsForMonth(shownMonth.getFullYear(), shownMonth.getMonth() + 1);
    }
    readonly property var selectedEvents: dayEvents(selectedDate)
    readonly property int shownEventLimit: 4

    implicitWidth: sideInset * 2 + cellSize * 7
    implicitHeight: topSpacing + content.implicitHeight + bottomSpacing

    function sameDay(first, second) {
        return first.getFullYear() === second.getFullYear()
            && first.getMonth() === second.getMonth()
            && first.getDate() === second.getDate();
    }

    function eventsOn(date) {
        revision;
        const found = backend.daysModel.eventsForDate(date);
        const result = [];
        for (let index = 0; index < found.length; ++index)
            result.push(found[index]);
        return result;
    }

    // Holidays first, as all-day events, then the linked calendars' own.
    function dayEvents(date) {
        const holidays = eventsOn(date).map(event => ({ title: event.title, allDay: true }));
        const inShownMonth = date.getFullYear() === shownMonth.getFullYear()
            && date.getMonth() === shownMonth.getMonth();
        const linked = inShownMonth ? (linkedEvents[String(date.getDate())] ?? []) : [];
        return holidays.concat(linked);
    }

    function reset() {
        shownMonth = new Date(today.getFullYear(), today.getMonth(), 1);
        selectedDate = today;
    }

    function showMonth(step) {
        shownMonth = new Date(shownMonth.getFullYear(), shownMonth.getMonth() + step, 1);
        selectedDate = showingCurrentMonth ? today : shownMonth;
        grid.arrive(step);
    }

    PlasmaCalendar.EventPluginsManager {
        id: eventPlugins
        enabledPlugins: ["holidaysevents"]
    }

    PlasmaCalendar.Calendar {
        id: backend
        days: 7
        weeks: 6
        firstDayOfWeek: page.firstWeekday
        today: page.today
        displayedDate: page.shownMonth
        Component.onCompleted: daysModel.setPluginsManager(eventPlugins)
    }

    Connections {
        target: backend.daysModel
        function onAgendaUpdated() { page.revision++; }
        function onDataChanged() { page.revision++; }
        function onModelReset() { page.revision++; }
    }

    ColumnLayout {
        id: content
        x: page.sideInset
        y: page.topSpacing
        width: page.cellSize * 7
        spacing: 0

        // The weekday initials sit as close to the grid as its rows sit to
        // each other: a circle's 4 px inset plus 4 px here makes the 8 px gap.
        Row {
            objectName: "temperance-calendar-weekdays"
            Layout.bottomMargin: 4
            Repeater {
                model: 7
                delegate: Text {
                    required property int index
                    width: page.cellSize
                    horizontalAlignment: Text.AlignHCenter
                    text: Qt.locale().dayName((page.firstWeekday + index) % 7, Locale.NarrowFormat)
                    font.family: Kirigami.Theme.defaultFont.family
                    font.pixelSize: 11
                    font.weight: Font.Medium
                    color: "#A8FFFFFF"
                }
            }
        }

        Item {
            Layout.preferredWidth: grid.width
            Layout.preferredHeight: grid.height

            Grid {
                id: grid
                objectName: "temperance-calendar-grid"
                columns: 7

                property real shift: 0

                // A new month slides in from the side it was asked for.
                function arrive(step) {
                    arrival.stop();
                    if (Kirigami.Units.longDuration <= 0)
                        return;
                    shift = step * 16;
                    opacity = 0;
                    arrival.restart();
                }

                transform: Translate { x: grid.shift }

                ParallelAnimation {
                    id: arrival
                    NumberAnimation {
                        target: grid; property: "shift"; to: 0
                        duration: Kirigami.Units.longDuration; easing.type: Easing.OutCubic
                    }
                    NumberAnimation {
                        target: grid; property: "opacity"; to: 1
                        duration: Kirigami.Units.longDuration; easing.type: Easing.OutCubic
                    }
                }

                Repeater {
                    model: page.cells
                    delegate: Item {
                        id: cell
                        required property int modelData
                        readonly property bool inMonth: modelData > 0
                        readonly property date date: new Date(page.shownMonth.getFullYear(),
                                                              page.shownMonth.getMonth(), Math.max(1, modelData))
                        readonly property bool isToday: inMonth && page.sameDay(date, page.today)
                        readonly property bool isSelected: inMonth && page.sameDay(date, page.selectedDate)
                        readonly property var events: inMonth ? page.dayEvents(date) : []

                        objectName: inMonth ? "temperance-calendar-day" : "temperance-calendar-blank"
                        width: page.cellSize
                        height: page.cellSize

                        Accessible.role: Accessible.Button
                        Accessible.name: inMonth ? [date.toLocaleDateString(Qt.locale(), Locale.LongFormat)]
                            .concat(events.map(event => event.title)).join(", ") : ""
                        Accessible.ignored: !inMonth
                        Accessible.onPressAction: page.selectedDate = date

                        Rectangle {
                            id: mark
                            objectName: "temperance-calendar-mark"
                            visible: cell.inMonth
                            anchors.centerIn: parent
                            width: page.markSize
                            height: page.markSize
                            radius: width / 2
                            color: cell.isToday ? page.accentColor
                                : cell.isSelected ? Qt.rgba(1, 1, 1, 0.24)
                                : dayHover.hovered ? Qt.rgba(1, 1, 1, 0.12) : "transparent"
                            Behavior on color {
                                ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
                            }
                        }

                        Text {
                            objectName: "temperance-calendar-number"
                            visible: cell.inMonth
                            anchors.centerIn: mark
                            text: cell.modelData
                            font.family: Kirigami.Theme.defaultFont.family
                            font.pixelSize: 15
                            font.weight: cell.isToday ? Font.Medium : Font.Normal
                            font.features: { "tnum": 1 }
                            color: cell.isToday ? "#102729" : "#F8F8FF"
                        }

                        // A day with anything on it carries a dot beneath its number.
                        Rectangle {
                            objectName: "temperance-calendar-dot"
                            visible: cell.inMonth && cell.events.length > 0
                            width: 4
                            height: 4
                            radius: 2
                            anchors.horizontalCenter: mark.horizontalCenter
                            y: mark.y + mark.height / 2 + 9
                            color: cell.isToday ? "#102729" : "#A8FFFFFF"
                        }

                        HoverHandler { id: dayHover; enabled: cell.inMonth }
                        TapHandler {
                            enabled: cell.inMonth
                            onTapped: page.selectedDate = cell.date
                        }
                    }
                }
            }

            // A wheel turns one month a notch, down for later. A touchpad's
            // fine steps add up to a notch, and one turn per gesture keeps its
            // momentum from running through the year.
            WheelHandler {
                target: null
                property real pending: 0
                property bool fine: false
                onWheel: event => {
                    const delta = event.angleDelta.y !== 0 ? event.angleDelta.y : event.angleDelta.x;
                    fine = event.pixelDelta.y !== 0 || event.pixelDelta.x !== 0;
                    if (fine && settleTurn.running) {
                        settleTurn.restart();
                        return;
                    }
                    pending += delta;
                    if (Math.abs(pending) < 120)
                        return;
                    page.showMonth(pending < 0 ? 1 : -1);
                    pending = 0;
                    if (fine)
                        settleTurn.restart();
                }
            }
            Timer { id: settleTurn; interval: 250 }

            // A sideways swipe across the days turns the month.
            DragHandler {
                target: null
                xAxis.enabled: true
                yAxis.enabled: false
                onActiveChanged: {
                    if (active || Math.abs(activeTranslation.x) < 40)
                        return;
                    page.showMonth(activeTranslation.x < 0 ? 1 : -1);
                }
            }
        }

        // The selected day and what is on it. The circle's 4 px inset plus
        // 16 px here leaves the 20 px that separates a group. Each event is a
        // row: its start, or all day, in a column as wide as the widest start,
        // then its title on the same baseline.
        ColumnLayout {
            objectName: "temperance-calendar-agenda"
            Layout.topMargin: 16
            Layout.leftMargin: page.textInset - page.sideInset
            Layout.rightMargin: page.textInset - page.sideInset
            Layout.fillWidth: true
            spacing: 4

            Text {
                Layout.fillWidth: true
                text: page.selectedDate.toLocaleDateString(Qt.locale(), "dddd, MMMM d")
                font.family: Kirigami.Theme.defaultFont.family
                font.pixelSize: 13
                font.weight: Font.Medium
                color: "#A8FFFFFF"
                elide: Text.ElideRight
            }

            TextMetrics {
                id: widestStart
                font.family: Kirigami.Theme.defaultFont.family
                font.pixelSize: 13
                text: page.formatTime(new Date(2026, 0, 5, 22, 58))
            }
            TextMetrics {
                id: allDayWidth
                font: widestStart.font
                text: i18nc("@info an event that lasts the whole day", "all day")
            }

            Repeater {
                model: page.selectedEvents.slice(0, page.selectedEvents.length > page.shownEventLimit
                    ? page.shownEventLimit - 1 : page.shownEventLimit)
                delegate: RowLayout {
                    id: eventRow
                    required property var modelData
                    objectName: "temperance-calendar-event"
                    Layout.fillWidth: true
                    spacing: 8
                    Accessible.role: Accessible.StaticText
                    Accessible.name: [startText.text, titleText.text].join(", ")

                    Text {
                        id: startText
                        Layout.preferredWidth: Math.ceil(Math.max(widestStart.advanceWidth, allDayWidth.advanceWidth))
                        Layout.alignment: Qt.AlignBaseline
                        text: eventRow.modelData.allDay ? allDayWidth.text : page.formatTime(eventRow.modelData.start)
                        font: widestStart.font
                        color: "#A8FFFFFF"
                    }
                    Text {
                        id: titleText
                        objectName: "temperance-calendar-event-title"
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignBaseline
                        text: eventRow.modelData.title
                        font.family: Kirigami.Theme.defaultFont.family
                        font.pixelSize: 15
                        color: "#F8F8FF"
                        elide: Text.ElideRight
                    }
                }
            }

            Text {
                objectName: "temperance-calendar-more"
                visible: page.selectedEvents.length > page.shownEventLimit
                text: i18ncp("@info events not listed", "%1 more", "%1 more",
                             page.selectedEvents.length - page.shownEventLimit + 1)
                font: widestStart.font
                color: "#A8FFFFFF"
            }

            Text {
                objectName: "temperance-calendar-empty"
                visible: page.selectedEvents.length === 0
                text: page.hasLinks ? i18n("Nothing scheduled") : i18n("No holidays")
                font.family: Kirigami.Theme.defaultFont.family
                font.pixelSize: 15
                color: "#88FFFFFF"
            }
        }
    }
}
