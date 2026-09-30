#!/usr/bin/env bash
set -euo pipefail

package_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# TEMPERANCE_ROOT stages the files under another folder instead of the system.
root="${TEMPERANCE_ROOT:-}"
plugin_name="studio.warbler.temperance.so"
plugin_target="${root}/usr/lib/qt6/plugins/plasma/applets/${plugin_name}"
icon_name="studio.warbler.temperance.png"
icon_target="${root}/usr/share/icons/hicolor/512x512/apps/${icon_name}"

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

# An earlier install leaves files in place, so their being there proves
# nothing: they must be this package's own.
if ! cmp -s "${package_dir}/${plugin_name}" "${plugin_target}" \
        || ! cmp -s "${package_dir}/${icon_name}" "${icon_target}"; then
    printf '%s\n' "Installation failed: the system's Temperance is not this package's." \
        "Run ./install-system.sh again and watch for errors above." >&2
    exit 1
fi

# The panel runs this build once it has loaded the file just installed, which
# is a new file on disk, not the one it had before.
panel_runs_this_build() {
    local pid inode tries=0
    inode="$(stat -c %i "${plugin_target}")"
    while (( tries < 40 )); do
        pid="$(systemctl --user show -p MainPID --value plasma-plasmashell.service 2>/dev/null || true)"
        if [[ -n "${pid}" && "${pid}" != "0" ]] \
                && awk -v path="${plugin_target}" -v inode="${inode}" \
                    '$6 == path && $5 == inode { found = 1 } END { exit !found }' "/proc/${pid}/maps" 2>/dev/null; then
            return 0
        fi
        sleep 0.25
        tries=$((tries + 1))
    done
    return 1
}

# A restarted shell has sometimes come back not knowing the current activity,
# and then shows no desktop on any screen, so no wallpaper, until the activity
# is announced again. Once the shell answers, make sure it knows.
announce_activity() {
    command -v qdbus6 >/dev/null 2>&1 || return 0
    local shell_activity="" tries=0
    while (( tries < 40 )); do
        if shell_activity="$(qdbus6 org.kde.plasmashell /PlasmaShell \
                org.kde.PlasmaShell.evaluateScript 'print(currentActivity())' 2>/dev/null)"; then
            break
        fi
        sleep 0.25
        tries=$((tries + 1))
    done
    if [[ -z "${shell_activity}" ]]; then
        local current
        current="$(qdbus6 org.kde.ActivityManager /ActivityManager/Activities \
            org.kde.ActivityManager.Activities.CurrentActivity 2>/dev/null || true)"
        [[ -n "${current}" ]] && qdbus6 org.kde.ActivityManager /ActivityManager/Activities \
            org.kde.ActivityManager.Activities.SetCurrentActivity "${current}" >/dev/null 2>&1 || true
    fi
}

# The panel loads a widget's plugin once, so it restarts to pick up the new
# build. Windows and the session stay as they are.
if [[ -z "${root}" ]] && systemctl --user --quiet is-active plasma-plasmashell.service 2>/dev/null; then
    systemctl --user restart plasma-plasmashell.service
    announce_activity
    if panel_runs_this_build; then
        printf '%s\n' "Installed Temperance and restarted the panel, which now runs this build."
    else
        printf '%s\n' "Installed Temperance and restarted the panel, but the panel has not loaded it yet." \
            "If Temperance is on a panel, sign out and back in to load it."
    fi
else
    printf '%s\n' "Installed Temperance successfully. Sign out and back in to load it."
fi
printf '%s\n' "The first time, add it to a panel from Add Widgets: Temperance"
