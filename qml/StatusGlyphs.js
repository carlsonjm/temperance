/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
.pragma library

// Which of the drawings Temperance carries (assets/icons/colloid, copied by
// tools/status-glyphs.py) draws a Plasma status icon, so Status looks the same
// whatever icon theme is installed. Plasma's widgets name their state by icon,
// such as network-wireless-60 or audio-volume-low-symbolic, and each name maps
// to a drawing shipped with Temperance. An application's own tray icon has no
// match and keeps its own picture.

function plainName(icon) {
    return String(icon || "").replace(/-rtl$/, "").replace(/-symbolic$/, "");
}

// plasma-nm's connection icon: a base name for the device and its signal,
// then -locked while a VPN is up or -limited without the internet. A
// passworded Wi-Fi network carries no lock: the lock means the VPN. A wired
// connection draws Lucide's ethernet port in the bell's line (suite-ethernet,
// EthernetGlyph.qml). Every other state has a drawing of its own, and a state
// Plasma adds later still draws as a network, never as the theme's.
function network(icon) {
    let name = plainName(icon);
    let badge = "";
    const suffix = /-(locked|limited)$/.exec(name);
    if (suffix) {
        badge = suffix[1];
        name = name.slice(0, -suffix[0].length);
    }
    if (/^network-wired(?:-activated)?$/.test(name)) return "suite-ethernet";
    const wireless = /^network-wireless-(?:connected-)?([0-9]+)$/.exec(name);
    if (wireless) {
        if (badge === "limited") return "network-wireless-no-route-symbolic";
        const signal = { "100": 100, "80": 100, "60": 75, "40": 50, "20": 25, "0": 0, "00": 0 }[wireless[1]];
        return "nm-signal-" + (signal === undefined ? 100 : signal)
            + (badge === "locked" ? "-secure" : "") + "-symbolic";
    }
    const mobile = /^network-mobile-([0-9]+)(?:-[a-z0-9]+)?$/.exec(name);
    if (mobile) {
        if (badge === "limited") return "network-cellular-no-route-symbolic";
        const bars = { "100": "excellent", "80": "excellent", "60": "good", "40": "ok",
            "20": "low", "0": "none" }[mobile[1]] || "excellent";
        return "network-cellular-signal-" + bars + "-symbolic";
    }
    const named = {
        "network-wireless": "nm-signal-100-symbolic",
        "network-wireless-acquiring": "network-wireless-acquiring-symbolic",
        "network-wireless-disconnected": "network-wireless-offline-symbolic",
        "network-wireless-off": "network-wireless-disabled-symbolic",
        "network-wireless-hotspot": "network-wireless-hotspot-symbolic",
        "network-wireless-bluetooth": "bluetooth-active-symbolic",
        "network-wired-disconnected": "network-wired-offline-symbolic",
        "network-mobile-available": "network-cellular-offline-symbolic",
        "network-mobile-off": "network-cellular-offline-symbolic",
        "network-modem": "network-cellular-signal-excellent-symbolic",
        "network-vpn": "network-vpn-symbolic",
        "network-flightmode-on": "airplane-mode-symbolic",
        "network-unavailable": "network-wired-offline-symbolic"
    };
    if (named[name]) return named[name];
    if (name.startsWith("network-mobile")) return "network-cellular-signal-excellent-symbolic";
    if (name.startsWith("network-wired")) return "suite-ethernet";
    return "network-wireless-offline-symbolic";
}

function battery(name) {
    if (name === "speedometer") return "power-profile-balanced-symbolic";
    if (name === "battery-missing") return "battery-missing-symbolic";
    const charging = /-charging$/.test(name) || name === "battery-full-charged";
    const level = /^battery-([0-9]{3})(?:-charging)?$/.exec(name);
    const named = { "battery-full": 100, "battery-full-charged": 100, "battery-good": 70,
        "battery-low": 30, "battery-caution": 10, "battery-empty": 10 };
    const percent = level ? Number(level[1]) : (named[name.replace(/-charging$/, "")] ?? 100);
    if (charging) {
        return percent >= 95 ? "battery-full-charging-symbolic"
            : "battery-level-" + (percent >= 80 ? 90 : percent >= 50 ? 70 : percent >= 20 ? 30 : 10)
                + "-charging-symbolic";
    }
    return percent >= 95 ? "battery-full-symbolic"
        : "battery-level-" + (percent >= 80 ? 90 : percent >= 60 ? 70 : percent >= 40 ? 50
            : percent >= 25 ? 30 : percent >= 15 ? 20 : 10) + "-symbolic";
}

// The icon a Plasma widget gives for itself, as a drawing Temperance carries,
// or "" where the widget is not one of Plasma's own status widgets.
function applet(icon) {
    const name = plainName(icon);
    if (name.startsWith("network-") && !name.startsWith("network-bluetooth")
        && name !== "network-disconnect") {
        const glyph = network(name);
        return glyph === "suite-ethernet" ? "" : glyph;
    }
    if (name.startsWith("battery") || name === "speedometer") return battery(name);
    if (name.startsWith("audio-volume")) {
        if (/muted/.test(name)) return "audio-volume-muted-symbolic";
        if (/low/.test(name)) return "audio-volume-low-symbolic";
        if (/medium/.test(name)) return "audio-volume-medium-symbolic";
        if (/warning|danger|overamplified/.test(name)) return "audio-volume-overamplified-symbolic";
        return "audio-volume-high-symbolic";
    }
    if (name.startsWith("microphone-sensitivity") || name.startsWith("audio-input-microphone")) {
        if (/muted/.test(name)) return "microphone-sensitivity-muted-symbolic";
        if (/low/.test(name)) return "audio-input-microphone-low-symbolic";
        if (/medium/.test(name)) return "audio-input-microphone-medium-symbolic";
        return "audio-input-microphone-high-symbolic";
    }
    if (name.startsWith("network-bluetooth") || name.startsWith("bluetooth"))
        return /inactive|disabled/.test(name) ? "bluetooth-disabled-symbolic" : "bluetooth-active-symbolic";
    if (name.startsWith("drive-removable-media-usb")) return "drive-removable-media-usb-symbolic";
    if (name === "device-notifier" || name.startsWith("drive-") || name.startsWith("media-optical"))
        return "drive-harddisk-symbolic";
    const named = {
        "redshift-status-on": "night-light-symbolic",
        "redshift-status-day": "night-light-symbolic",
        "klipper": "edit-paste-symbolic",
        "printer": "printer-symbolic",
        "notification-active": "notification-symbolic",
        "notification-inactive": "notification-symbolic",
        "notifications-disabled": "notification-disabled-symbolic"
    };
    return named[name] || "";
}
