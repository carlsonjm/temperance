/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
pragma ComponentBehavior: Bound

import QtQuick
import org.kde.kirigami as Kirigami

// The time over the date, right-aligned; either can be hidden, and a line shown
// alone takes the primary style. The day period is a label, so it is lowercase,
// lighter and smaller than the time, and sits on the time's baseline. The
// font's own tabular figures are asked for, and each digit also sits in a cell
// as wide as the widest digit for fonts that have none. The item reserves the
// widest time and date the locale can produce, so neither a new minute nor a
// new day moves the layout.
Item {
    id: clock

    property date dateTime: new Date()
    // Empty follows the system UI font.
    property string fontFamily: ""
    property var clockLocale: Qt.locale()
    property bool showTime: true
    property bool showDate: true
    // Plasma's animation speed set to instant stands in for reduced motion.
    property bool reducedMotion: Kirigami.Units.longDuration <= 1

    readonly property bool twoLines: showTime && showDate
    readonly property bool dateIsPrimary: showDate && !showTime

    readonly property string family: fontFamily.length > 0
        ? fontFamily : Kirigami.Theme.defaultFont.family
    readonly property font primaryFont: Qt.font({ family: family, pixelSize: 15,
        weight: Font.Normal, features: { "tnum": 1 } })
    readonly property font periodFont: Qt.font({ family: family, pixelSize: 11,
        weight: Font.Light })
    readonly property font secondaryFont: Qt.font({ family: family, pixelSize: 11,
        weight: Font.Normal, features: { "tnum": 1 } })
    readonly property color primaryColor: "#F8F8FF"
    readonly property color secondaryColor: "#A8FFFFFF"

    readonly property string timeText: timeString(dateTime)
    readonly property string dateText: formatDate(dateTime)
    readonly property string longDateText: dateTime.toLocaleDateString(clockLocale, Locale.LongFormat)

    // English writes the day period a.m. and p.m.; another language's marker
    // is only lowercased.
    readonly property string amLabel: periodLabel(clockLocale.amText)
    readonly property string pmLabel: periodLabel(clockLocale.pmText)

    // A new minute is dealt onto the old one like a card onto a pile: it
    // arrives from above, nearer the eye and slightly turned, and lands flat
    // while the old glyph darkens under it and sinks into the pile. Glyphs have
    // no solid back to cover with, so the old one is gone before the two
    // overlap enough to smudge. A small drop
    // with a crossfade reads as a bump, so the card travels the spatial tier.
    // No overshoot, since the time is a fact. Under reduced motion it only
    // fades. The pace ignores Plasma's animation speed: that setting shortens
    // what a person waits on, nobody waits on the clock, and a quarter-length
    // deal is too quick to see.
    readonly property int dealDuration: reducedMotion ? 160 : 260
    readonly property int leaveDuration: reducedMotion ? 160 : 150
    readonly property real dealScale: reducedMotion ? 1 : 1.22
    readonly property real sinkScale: reducedMotion ? 1 : 0.88
    readonly property real dealTilt: reducedMotion ? 0 : -8

    readonly property string digits: {
        const zero = clockLocale.zeroDigit.charCodeAt(0);
        let result = "0123456789";
        for (let i = 0; i < 10; ++i)
            result += String.fromCharCode(zero + i);
        return result;
    }
    // Day before month where the locale's own short date puts it first.
    readonly property bool dayFirst: {
        const format = clockLocale.dateFormat(Locale.ShortFormat);
        const day = format.indexOf("d");
        const month = format.indexOf("M");
        return day >= 0 && month >= 0 && day < month;
    }

    readonly property real primaryCell: { primaryMetrics.font; return widestDigit(primaryMetrics); }
    readonly property real secondaryCell: { secondaryMetrics.font; return widestDigit(secondaryMetrics); }
    // A two-digit hour on each side of noon covers every length the time
    // format has, and both day-period names where it has them.
    readonly property var timeSamples: [10, 22].map(hour => timeString(new Date(2026, 0, 5, hour, 8)))
    // Whatever follows the last digit, usually AM or PM, takes the width of the
    // widest, so the minutes stay where they are when the day period changes.
    readonly property real timeSuffixWidth: {
        primaryMetrics.font;
        periodMetrics.font;
        let widest = 0;
        for (const sample of timeSamples) {
            const parts = tokens(sample);
            const last = parts[parts.length - 1];
            if (last && !last.digit)
                widest = Math.max(widest, advance(last, primaryMetrics));
        }
        return widest;
    }
    readonly property real reservedTimeWidth: {
        primaryMetrics.font;
        periodMetrics.font;
        let widest = 0;
        for (const sample of timeSamples)
            widest = Math.max(widest, lineWidth(tokens(sample), primaryMetrics, primaryCell, timeSuffixWidth));
        return widest;
    }
    readonly property real reservedDateWidth: {
        primaryMetrics.font;
        secondaryMetrics.font;
        return dateIsPrimary ? widestDate(primaryMetrics, primaryCell)
                             : widestDate(secondaryMetrics, secondaryCell);
    }

    // Stacked by glyph rather than by line box: a UI font's line box carries
    // enough leading that two of them overfill the bar. The height runs from
    // the top of the first line's digits to the last line's baseline, so the
    // clock centres optically and descenders hang into the margin below.
    readonly property real primaryCapHeight: { primaryMetrics.font; return primaryMetrics.tightBoundingRect("0").height; }
    readonly property real secondaryCapHeight: { secondaryMetrics.font; return secondaryMetrics.tightBoundingRect("0").height; }
    readonly property real lineGap: 4

    function periodLabel(marker) {
        if (marker === "AM")
            return "a.m.";
        if (marker === "PM")
            return "p.m.";
        return marker.toLowerCase();
    }

    function timeString(value) {
        const text = value.toLocaleTimeString(clockLocale, Locale.ShortFormat);
        for (const [marker, label] of [[clockLocale.amText, amLabel], [clockLocale.pmText, pmLabel]]) {
            if (marker.length > 0 && text.indexOf(marker) !== -1)
                return text.replace(marker, label);
        }
        return text;
    }

    function widestDigit(metrics) {
        let widest = 0;
        for (const digit of digits)
            widest = Math.max(widest, metrics.advanceWidth(digit));
        return widest;
    }

    function widestDate(metrics, cell) {
        let weekday = 0;
        for (let day = 0; day < 7; ++day)
            weekday = Math.max(weekday, metrics.advanceWidth(clockLocale.dayName(day, Locale.ShortFormat)));
        let month = 0;
        for (let index = 0; index < 12; ++index)
            month = Math.max(month, metrics.advanceWidth(clockLocale.monthName(index, Locale.ShortFormat)));
        return weekday + month + metrics.advanceWidth(dateParts("", "", "")) + 2 * cell;
    }

    // Each digit is its own token; a run of anything else stays whole so its
    // kerning survives. The run that is the day period is marked for its
    // lighter weight.
    function tokens(text) {
        const result = [];
        for (const character of text) {
            const digit = digits.indexOf(character) !== -1;
            if (!digit && result.length > 0 && !result[result.length - 1].digit)
                result[result.length - 1].text += character;
            else
                result.push({ text: character, digit: digit });
        }
        for (const part of result) {
            const label = part.text.trim();
            part.period = !part.digit && label.length > 0 && (label === amLabel || label === pmLabel);
        }
        return result;
    }

    function advance(part, metrics) {
        return (part.period ? periodMetrics : metrics).advanceWidth(part.text);
    }

    function tokenWidth(parts, index, metrics, cell, suffixWidth) {
        const part = parts[index];
        if (!part)
            return 0;
        if (part.digit)
            return cell;
        const width = advance(part, metrics);
        return index === parts.length - 1 ? Math.max(width, suffixWidth) : width;
    }

    function lineWidth(parts, metrics, cell, suffixWidth) {
        let width = 0;
        for (let index = 0; index < parts.length; ++index)
            width += tokenWidth(parts, index, metrics, cell, suffixWidth);
        return width;
    }

    function dateParts(weekday, month, day) {
        return dayFirst ? weekday + " " + day + " " + month
                        : weekday + ", " + month + " " + day;
    }

    function formatDate(value) {
        return dateParts(clockLocale.dayName(value.getDay(), Locale.ShortFormat),
                         clockLocale.monthName(value.getMonth(), Locale.ShortFormat),
                         value.toLocaleDateString(clockLocale, "d"));
    }

    implicitWidth: Math.ceil(Math.max(showTime ? reservedTimeWidth : 0,
                                      showDate ? reservedDateWidth : 0))
    implicitHeight: !showTime && !showDate ? 0
        : Math.ceil(twoLines ? primaryCapHeight + lineGap + secondaryCapHeight : primaryCapHeight)

    Accessible.role: Accessible.StaticText
    Accessible.name: [showTime ? timeText : "", showDate ? longDateText : ""]
        .filter(part => part.length > 0).join(", ")

    FontMetrics { id: primaryMetrics; font: clock.primaryFont }
    FontMetrics { id: secondaryMetrics; font: clock.secondaryFont }
    FontMetrics { id: periodMetrics; font: clock.periodFont }

    component ClockLine: Row {
        id: line
        required property string text
        required property font lineFont
        required property color lineColor
        required property FontMetrics metrics
        required property real cell
        required property real capHeight
        property real suffixWidth: 0
        readonly property var parts: clock.tokens(text)
        readonly property real travel: clock.reducedMotion ? 0 : Math.round(capHeight * 1.1)

        readonly property real digitBaseline: reference.baselineOffset

        anchors.right: parent.right

        // A Row skips a hidden child, so this only measures.
        Text {
            id: reference
            visible: false
            text: "0"
            font: line.lineFont
        }

        // Counted rather than modelled on the tokens, so a token keeps its
        // item across a change and can deal the new glyph onto the old one.
        Repeater {
            model: line.parts.length
            delegate: Item {
                id: token
                required property int index
                readonly property var part: line.parts[index] ?? { text: "", digit: false, period: false }
                readonly property bool digit: part.digit
                readonly property font tokenFont: part.period ? clock.periodFont : line.lineFont
                property string shown: ""

                objectName: digit ? "temperance-clock-digit" : "temperance-clock-text"
                // Measured from the drawn text rather than the font's metrics,
                // since a face missing a weight falls back to another font.
                y: part.period ? line.digitBaseline - glyph.baselineOffset : 0
                width: clock.tokenWidth(line.parts, index, line.metrics, line.cell, line.suffixWidth)
                height: glyph.implicitHeight

                Component.onCompleted: shown = part.text
                onPartChanged: {
                    if (part.text === shown)
                        return;
                    outgoing.text = shown;
                    shown = part.text;
                    deal.restart();
                }

                Text {
                    id: outgoing
                    objectName: "temperance-clock-outgoing"
                    x: token.digit ? (token.width - implicitWidth) / 2 : 0
                    opacity: 0
                    font: token.tokenFont
                    color: line.lineColor
                }
                Text {
                    id: glyph
                    objectName: "temperance-clock-glyph"
                    x: token.digit ? (token.width - implicitWidth) / 2 : 0
                    text: token.shown
                    font: token.tokenFont
                    color: line.lineColor
                }

                // The card is solid almost as soon as it is in the air.
                ParallelAnimation {
                    id: deal
                    NumberAnimation {
                        target: glyph; property: "y"
                        from: -line.travel; to: 0
                        duration: clock.dealDuration; easing.type: Easing.OutCubic
                    }
                    NumberAnimation {
                        target: glyph; property: "scale"
                        from: clock.dealScale; to: 1
                        duration: clock.dealDuration; easing.type: Easing.OutCubic
                    }
                    NumberAnimation {
                        target: glyph; property: "rotation"
                        from: clock.dealTilt; to: 0
                        duration: clock.dealDuration; easing.type: Easing.OutCubic
                    }
                    NumberAnimation {
                        target: glyph; property: "opacity"
                        from: 0; to: 1
                        duration: clock.reducedMotion ? clock.dealDuration : Math.round(clock.dealDuration * 0.35)
                        easing.type: Easing.OutCubic
                    }
                    NumberAnimation {
                        target: outgoing; property: "opacity"
                        from: 1; to: 0
                        duration: clock.leaveDuration; easing.type: Easing.OutCubic
                    }
                    NumberAnimation {
                        target: outgoing; property: "scale"
                        from: 1; to: clock.sinkScale
                        duration: clock.leaveDuration; easing.type: Easing.OutCubic
                    }
                }
            }
        }
    }

    Item {
        id: lines
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        height: clock.implicitHeight

        ClockLine {
            objectName: "temperance-clock-time"
            visible: clock.showTime
            y: clock.primaryCapHeight - primaryMetrics.ascent
            text: clock.timeText
            lineFont: clock.primaryFont
            lineColor: clock.primaryColor
            metrics: primaryMetrics
            cell: clock.primaryCell
            capHeight: clock.primaryCapHeight
            suffixWidth: clock.timeSuffixWidth
        }
        ClockLine {
            objectName: "temperance-clock-date"
            visible: clock.showDate
            y: clock.twoLines
                ? clock.primaryCapHeight + clock.lineGap + clock.secondaryCapHeight - secondaryMetrics.ascent
                : clock.primaryCapHeight - primaryMetrics.ascent
            text: clock.dateText
            lineFont: clock.dateIsPrimary ? clock.primaryFont : clock.secondaryFont
            lineColor: clock.dateIsPrimary ? clock.primaryColor : clock.secondaryColor
            metrics: clock.dateIsPrimary ? primaryMetrics : secondaryMetrics
            cell: clock.dateIsPrimary ? clock.primaryCell : clock.secondaryCell
            capHeight: clock.dateIsPrimary ? clock.primaryCapHeight : clock.secondaryCapHeight
        }
    }
}
