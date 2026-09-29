#!/usr/bin/env bash
set -euo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/itasca-center-panel.*/runtime ]]
export QT_QPA_PLATFORM=wayland
export QT_SCALE_FACTOR="$ITASCA_TEST_SCALE"
exec "$ITASCA_CENTER_TEST_BINARY"
