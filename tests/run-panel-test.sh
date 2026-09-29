#!/usr/bin/env bash
set -euo pipefail
test_root="$(mktemp -d /tmp/temperance-panel-test.XXXXXX)"
trap 'rm -rf -- "$test_root"' EXIT
mkdir -p "$test_root/runtime" "$test_root/config" "$test_root/data" "$test_root/cache"
chmod 700 "$test_root/runtime"
export XDG_RUNTIME_DIR="$test_root/runtime"
export XDG_CONFIG_HOME="$test_root/config"
export XDG_DATA_HOME="$test_root/data"
export XDG_CACHE_HOME="$test_root/cache"
export QT_QPA_PLATFORM=offscreen
export QT_QPA_PLATFORMTHEME=generic
export QT_STYLE_OVERRIDE=Fusion
export QT_QUICK_BACKEND=software
export QT_PLUGIN_PATH="$2"
dbus-run-session --config-file="$(dirname "$0")/panel-test-bus.conf" -- "$1"
