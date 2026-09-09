#!/usr/bin/env bash
set -euo pipefail

plugin_target="/usr/lib/qt6/plugins/plasma/applets/studio.warbler.temperance.so"
icon_target="/usr/share/icons/hicolor/512x512/apps/studio.warbler.temperance.png"

sudo rm -f -- "${plugin_target}" "${icon_target}"
kbuildsycoca6

printf '%s\n' "Removed Temperance. Restart Plasma or sign out to finish."
