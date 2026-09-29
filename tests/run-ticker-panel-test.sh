#!/usr/bin/env bash
set -euo pipefail
center_root=$(mktemp -d /tmp/itasca-center-panel.XXXXXX)
mkdir -p "$center_root/runtime" "$center_root/config" "$center_root/data" "$center_root/cache"
chmod 700 "$center_root/runtime"
test -f "$2/plasma/applets/studio.warbler.tettegouche.so"
test -f "$3/plasma/applets/studio.warbler.temperance.so"
echo "Temperance T1 evidence: $center_root"
env XDG_RUNTIME_DIR="$center_root/runtime" XDG_CONFIG_HOME="$center_root/config" \
    XDG_DATA_HOME="$center_root/data" XDG_CACHE_HOME="$center_root/cache" \
    QT_QPA_PLATFORMTHEME=generic QT_STYLE_OVERRIDE=Fusion \
    QT_QUICK_BACKEND=software QT_PLUGIN_PATH="$2:$3" \
    ITASCA_TEST_TETTE_PLUGIN="$2/plasma/applets/studio.warbler.tettegouche.so" \
    ITASCA_TEST_TEMPERANCE_PLUGIN="$3/plasma/applets/studio.warbler.temperance.so" \
    ITASCA_TEST_SCALE="${6:-1}" ITASCA_TEST_SCREEN="${7:-0}" \
    ITASCA_CENTER_TEST_BINARY="$1" \
    KWIN_COMPOSE=O2 LIBGL_ALWAYS_SOFTWARE=1 QT_WAYLAND_RECONNECT=0 \
    timeout 45s dbus-run-session -- kwin_wayland --virtual --width "${4:-1280}" --height "${5:-800}" \
    --output-count 2 --no-lockscreen --no-global-shortcuts --no-kactivities \
    --exit-with-session "$(cd -- "$(dirname "$0")" && pwd)/ticker-panel-session.sh" \
    > "$center_root/test.log" 2>&1
rg 'PASS|FAIL|T1_|TICKER|PAINTED|DISCLOSURE|Totals' "$center_root/test.log"
