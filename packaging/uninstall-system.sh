#!/usr/bin/env bash
set -euo pipefail

plugin_target="/usr/lib/qt6/plugins/plasma/applets/co.goodinput.temperance.so"
icon_target="/usr/share/icons/hicolor/512x512/apps/co.goodinput.temperance.png"

install_key=/usr/local/libexec/shuffle/install-step
if [[ -x "${install_key}" ]] \
        && sudo -n -l "${install_key}" temperance remove >/dev/null 2>&1; then
    sudo -n "${install_key}" temperance remove
else
    sudo rm -f -- "${plugin_target}" "${icon_target}"
fi
kbuildsycoca6

printf '%s\n' "Removed Temperance. Restart Plasma or sign out to finish."
