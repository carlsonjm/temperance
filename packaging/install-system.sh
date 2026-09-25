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

# Linked calendars are read with KDE's calendar library, which a Plasma
# desktop does not always carry.
if ! ldconfig -p | grep -q 'libKF6CalendarCore\.so\.6'; then
    printf '%s\n' "Temperance needs KDE's calendar library (the kcalendarcore package)." \
        "On Arch and CachyOS: sudo pacman -S kcalendarcore" >&2
    exit 1
fi

sudo install -Dm755 "${package_dir}/${plugin_name}" "${plugin_target}"
sudo install -Dm644 "${package_dir}/${icon_name}" "${icon_target}"
kbuildsycoca6

if [[ ! -f "${plugin_target}" || ! -f "${icon_target}" ]]; then
    printf '%s\n' "Installation failed: the plugin or icon was not created." >&2
    exit 1
fi

# The panel loads a widget's plugin once, so it restarts to pick up the new
# build. Windows and the session stay as they are.
if systemctl --user --quiet is-active plasma-plasmashell.service 2>/dev/null; then
    systemctl --user restart plasma-plasmashell.service
    printf '%s\n' "Installed Temperance and restarted the panel."
else
    printf '%s\n' "Installed Temperance successfully. Sign out and back in to load it."
fi
printf '%s\n' "The first time, add it to a panel from Add Widgets: Temperance"
