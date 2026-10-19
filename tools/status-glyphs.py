#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 carlsonjm
# SPDX-License-Identifier: ISC
"""Copy the status icons Status draws from a checkout of the Colloid icon theme.

Status carries these drawings itself, so it looks the same whatever icon theme
is installed. They are Colloid's bold panel set, the one the Status Bar was
designed against, taken from the pinned commit below; where the bold set has
no drawing, Colloid's regular one. Colloid draws in #363636 for a light theme
and its installer repaints that for a dark one; the copies here are painted
Ghost White, as every glyph SuiteIcon shows is, and recoloured for a light
style at run time. Geometry is unchanged.

    git clone https://github.com/vinceliuice/Colloid-icon-theme.git colloid
    git -C colloid checkout ceac6608ecd0e40025cbc2ebbd32bf0e0f4ebc6a
    python3 tools/status-glyphs.py colloid   # rewrites assets/icons/colloid/
"""
import pathlib
import subprocess
import sys

COMMIT = "ceac6608ecd0e40025cbc2ebbd32bf0e0f4ebc6a"
ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "assets/icons/colloid"

# The names StatusGlyphs.js gives, each read under Colloid's own name.
NAMES = [
    # Wi-Fi, by Colloid's five signal steps, with the lock shown while a VPN is up.
    *[f"nm-signal-{level}{lock}-symbolic" for level in (0, 25, 50, 75, 100) for lock in ("", "-secure")],
    "network-wireless-acquiring-symbolic", "network-wireless-offline-symbolic",
    "network-wireless-disabled-symbolic", "network-wireless-no-route-symbolic",
    "network-wireless-hotspot-symbolic", "network-wired-offline-symbolic",
    "network-wired-no-route-symbolic", "network-vpn-symbolic", "airplane-mode-symbolic",
    *[f"network-cellular-signal-{level}-symbolic" for level in ("none", "low", "ok", "good", "excellent")],
    "network-cellular-offline-symbolic", "network-cellular-no-route-symbolic",
    # Control Center's pills.
    "audio-volume-muted-symbolic", "audio-volume-low-symbolic", "audio-volume-medium-symbolic",
    "audio-volume-high-symbolic", "audio-volume-overamplified-symbolic",
    "audio-input-microphone-low-symbolic", "audio-input-microphone-medium-symbolic",
    "audio-input-microphone-high-symbolic", "microphone-sensitivity-muted-symbolic",
    "bluetooth-active-symbolic", "bluetooth-disabled-symbolic",
    *[f"battery-level-{level}-symbolic" for level in (10, 20, 30, 50, 70, 90)],
    *[f"battery-level-{level}-charging-symbolic" for level in (10, 30, 70, 90)],
    "battery-full-symbolic", "battery-full-charging-symbolic", "battery-missing-symbolic",
    "power-profile-balanced-symbolic", "night-light-symbolic", "edit-paste-symbolic",
    "notification-symbolic", "notification-disabled-symbolic",
    "printer-symbolic", "drive-harddisk-symbolic", "drive-removable-media-usb-symbolic",
]


def find(colloid, name):
    for kind in ("status", "devices", "actions", "apps"):
        for tree in ("bold", "src"):
            path = colloid / tree / kind / "symbolic" / f"{name}.svg"
            if path.exists():
                return path
        link = colloid / "links" / kind / "symbolic" / f"{name}.svg"
        if link.is_symlink():
            target = colloid / "src" / kind / "symbolic" / link.readlink()
            bold = colloid / "bold" / kind / "symbolic" / link.readlink()
            return bold if bold.exists() else target
    raise SystemExit(f"Colloid has no {name}")


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    colloid = pathlib.Path(sys.argv[1])
    head = subprocess.run(["git", "-C", str(colloid), "rev-parse", "HEAD"],
                          capture_output=True, text=True, check=True).stdout.strip()
    if head != COMMIT:
        raise SystemExit(f"Colloid is at {head}, not the pinned {COMMIT}")
    OUT.mkdir(exist_ok=True)
    for old in OUT.glob("*.svg"):
        old.unlink()
    for name in NAMES:
        text = find(colloid, name).read_text().replace("#363636", "#F8F8FF")
        (OUT / f"{name}.svg").write_text(text)
        print(f"        assets/icons/colloid/{name}.svg")


if __name__ == "__main__":
    main()
