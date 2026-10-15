/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

import QtQuick
import org.kde.plasma.plasmoid

// The temperature the weather icon shows. It reads Plasma's Weather widget,
// and when that gives none and the online fallback is allowed, asks
// Open-Meteo for the widget's place.
Item {
    id: reading
    visible: false

    // The Weather widget's root item in the tray, or null.
    required property var applet
    required property bool active
    // weather, fahrenheit or celsius.
    required property string unit
    required property bool onlineFallback

    property string current: ""
    property bool pending: false
    readonly property string text: {
        if (current) return current;
        if (!applet) return "—°";
        const observation = applet.fullRepresentationItem
            ? applet.fullRepresentationItem.lastObservation : null;
        const nativeTemperature = observation ? Number(observation.temperature) : NaN;
        if (Number.isFinite(nativeTemperature)) return Math.round(nativeTemperature) + "°";
        const tooltip = String(applet.toolTipSubText || "").replace(/<[^>]*>/g, " ");
        const match = tooltip.match(/-?[0-9]+(?:\.[0-9]+)?\s*°/);
        return match ? match[0].replace(/\s+/g, "") : "—°";
    }

    function fetchCurrentTemperature(latitude, longitude) {
        const request = new XMLHttpRequest();
        request.onreadystatechange = function() {
            if (request.readyState !== XMLHttpRequest.DONE) return;
            reading.pending = false;
            if (request.status < 200 || request.status >= 300) return;
            try {
                const payload = JSON.parse(request.responseText);
                const temperature = Number(payload.current.temperature_2m);
                if (Number.isFinite(temperature)) reading.current = Math.round(temperature) + "°";
            } catch (error) {
                console.warn("Temperance: unable to parse current weather", error);
            }
        };
        const apiTemperatureUnit = unit === "celsius" ? "celsius" : "fahrenheit";
        request.open("GET", "https://api.open-meteo.com/v1/forecast?latitude="
            + encodeURIComponent(latitude) + "&longitude=" + encodeURIComponent(longitude)
            + "&current=temperature_2m&temperature_unit=" + apiTemperatureUnit, true);
        request.send();
    }

    function request() {
        if (pending) return;
        if (!applet) return;
        const page = applet.fullRepresentationItem;
        const observation = page ? page.lastObservation : null;
        const nativeTemperature = observation ? Number(observation.temperature) : NaN;
        if (unit === "weather" && Number.isFinite(nativeTemperature)) {
            current = Math.round(nativeTemperature) + "°";
            return;
        }
        if (!onlineFallback) return;

        const station = page ? page.station : null;
        const latitude = station ? Number(station.latitude) : NaN;
        const longitude = station ? Number(station.longitude) : NaN;
        pending = true;
        if (Number.isFinite(latitude) && Number.isFinite(longitude)) {
            fetchCurrentTemperature(latitude, longitude);
            return;
        }

        // Some providers expose only a display location. Resolve that once,
        // then use the same lightweight current-temperature request.
        const location = String(applet.toolTipMainText || "").trim();
        if (!location) {
            pending = false;
            return;
        }
        const geocode = new XMLHttpRequest();
        geocode.onreadystatechange = function() {
            if (geocode.readyState !== XMLHttpRequest.DONE) return;
            if (geocode.status < 200 || geocode.status >= 300) {
                reading.pending = false;
                return;
            }
            try {
                const payload = JSON.parse(geocode.responseText);
                const result = payload.results && payload.results.length ? payload.results[0] : null;
                if (result) {
                    reading.fetchCurrentTemperature(Number(result.latitude), Number(result.longitude));
                    return;
                }
            } catch (error) {
                console.warn("Temperance: unable to resolve weather location", error);
            }
            reading.pending = false;
        };
        geocode.open("GET", "https://geocoding-api.open-meteo.com/v1/search?count=1&language=en&format=json&name="
            + encodeURIComponent(location), true);
        geocode.send();
    }

    function restart() {
        current = "";
        pending = false;
        attempts = 0;
        Qt.callLater(request);
    }
    onUnitChanged: restart()
    onOnlineFallbackChanged: restart()

    // Until a temperature arrives, ask again after 5 s, 30 s, 2 min and
    // 10 min, then once an hour, the same as once one has arrived. Nothing runs without a
    // weather widget, and a new widget or setting starts the steps over.
    property int attempts: 0
    readonly property var retryDelays: [5000, 5000, 30000, 120000, 600000, 3600000]

    Timer {
        interval: reading.current ? 3600000
            : reading.retryDelays[Math.min(reading.attempts, reading.retryDelays.length - 1)]
        running: reading.active && !!reading.applet
        repeat: true
        triggeredOnStart: true
        onRunningChanged: if (running) reading.attempts = 0
        onTriggered: {
            reading.attempts += 1;
            reading.request();
        }
    }
}
