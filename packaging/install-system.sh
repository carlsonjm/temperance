#!/usr/bin/env bash
set -euo pipefail

package_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
plugin_name="studio.warbler.temperance.so"
plugin_target="/usr/lib/qt6/plugins/plasma/applets/${plugin_name}"
icon_name="studio.warbler.temperance.png"
icon_target="/usr/share/icons/hicolor/512x512/apps/${icon_name}"

# File managers launch scripts without a terminal, which makes sudo fail out of
# sight. Re-open ourselves in a terminal so authorization and errors are visible.
if [[ ! -t 0 && "${TEMPERANCE_IN_TERMINAL:-0}" != "1" ]]; then
    if command -v konsole >/dev/null 2>&1; then
        exec konsole --hold -e env TEMPERANCE_IN_TERMINAL=1 bash "${BASH_SOURCE[0]}"
    elif command -v xterm >/dev/null 2>&1; then
        exec xterm -hold -e env TEMPERANCE_IN_TERMINAL=1 bash "${BASH_SOURCE[0]}"
    else
        printf '%s\n' "Open this folder in a terminal and run: ./install-system.sh" >&2
        exit 1
    fi
fi

if [[ ! -f "${package_dir}/${plugin_name}" ]]; then
    printf 'Missing plugin file: %s\n' "${package_dir}/${plugin_name}" >&2
    exit 1
fi
if [[ ! -f "${package_dir}/${icon_name}" ]]; then
    printf 'Missing icon file: %s\n' "${package_dir}/${icon_name}" >&2
    exit 1
fi

sudo install -Dm755 "${package_dir}/${plugin_name}" "${plugin_target}"
sudo install -Dm644 "${package_dir}/${icon_name}" "${icon_target}"
kbuildsycoca6

if [[ ! -f "${plugin_target}" || ! -f "${icon_target}" ]]; then
    printf '%s\n' "Installation failed: the plugin or icon was not created." >&2
    exit 1
fi

printf '%s\n' "Installed Temperance successfully."
printf '%s\n' "Sign out and back in, then search Add Widgets for: Temperance"
