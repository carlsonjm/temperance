#!/usr/bin/env bash
set -euo pipefail

# TEMPERANCE_ROOT removes the files from another folder instead of the system.
root="${TEMPERANCE_ROOT:-}"

# Qt looks for plugins where it was built to, as the installer asks it.
qt_plugins() {
    local dir=""
    if command -v qtpaths6 >/dev/null 2>&1; then
        dir="$(qtpaths6 --query QT_INSTALL_PLUGINS 2>/dev/null || true)"
    fi
    if [[ "${dir}" != /* ]] && command -v qmake6 >/dev/null 2>&1; then
        dir="$(qmake6 -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
    fi
    [[ "${dir}" == /* ]] || dir=/usr/lib/qt6/plugins
    printf '%s\n' "${dir%/}"
}
plugin_dir="$(qt_plugins)"
plugin_target="${root}${plugin_dir}/plasma/applets/co.goodinput.temperance.so"
icon_target="${root}/usr/share/icons/hicolor/512x512/apps/co.goodinput.temperance.png"

# Temperance shows the notifications and holds the tray, so a panel left
# without it would show neither. While the panel still runs it, each panel
# that holds Temperance gets Plasma's own System Tray back in its place,
# which brings Plasma's notifications with it.
panel_shell_running() {
    [[ -z "${root}" ]] && command -v qdbus6 >/dev/null 2>&1 \
        && systemctl --user --quiet is-active plasma-plasmashell.service 2>/dev/null
}
restored=""
if panel_shell_running; then
    restored="$(qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript '
        var restored = 0;
        panels().forEach(function (panel) {
            var held = false, tray = false;
            panel.widgets().forEach(function (widget) {
                if (widget.type === "co.goodinput.temperance") { widget.remove(); held = true; }
                else if (widget.type === "org.kde.plasma.systemtray") tray = true;
            });
            if (held && !tray) panel.addWidget("org.kde.plasma.systemtray");
            if (held) restored += 1;
        });
        print(restored);' 2>/dev/null || true)"
fi

install_key=/usr/local/libexec/shuffle/install-step
if [[ -z "${root}" && -x "${install_key}" && "${plugin_dir}" == /usr/lib/qt6/plugins ]] \
        && sudo -n -l "${install_key}" temperance remove >/dev/null 2>&1; then
    sudo -n "${install_key}" temperance remove
else
    sudo rm -f -- "${plugin_target}" "${icon_target}"
fi
kbuildsycoca6

# The panel keeps a widget's plugin loaded until it restarts.
if panel_shell_running; then
    systemctl --user restart plasma-plasmashell.service
    if [[ "${restored}" =~ ^[1-9] ]]; then
        printf '%s\n' "Removed Temperance and put Plasma's System Tray back on its panel, so notifications show again."
    else
        printf '%s\n' "Removed Temperance and restarted the panel." \
            "If no panel shows notifications now, add a System Tray from Add Widgets."
    fi
else
    printf '%s\n' "Removed Temperance. Sign out and back in to finish." \
        "If no panel shows notifications then, add a System Tray from Add Widgets."
fi
