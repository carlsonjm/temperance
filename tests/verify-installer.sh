#!/usr/bin/env bash
# The installer proves its install: this package's own files on the system,
# not merely files there. It runs against a staging root, with stand-ins for
# sudo and the desktop's tools.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
work="$(mktemp -d "${TMPDIR:-/tmp}/temperance-installer.XXXXXX")"
trap 'rm -rf -- "${work}"' EXIT
package="${work}/package" bin="${work}/bin" root="${work}/root"
mkdir -p "${package}" "${bin}" "${root}"
cp "${here}/../packaging/install-system.sh" "${package}/"
printf 'plugin' > "${package}/co.goodinput.temperance.so"
printf 'icon' > "${package}/co.goodinput.temperance.png"
printf '#!/bin/sh\necho "libKF6CalendarCore.so.6 (libc6,x86-64) => /usr/lib/libKF6CalendarCore.so.6"\n' > "${bin}/ldconfig"
printf '#!/bin/sh\nexit 0\n' > "${bin}/kbuildsycoca6"
printf '#!/bin/sh\nexit 3\n' > "${bin}/systemctl"
printf '#!/bin/sh\nexec "$@"\n' > "${bin}/sudo"
printf '#!/bin/sh\necho "plasmashell 6.7.4"\n' > "${bin}/plasmashell"
printf '#!/bin/sh\necho "6.11.2"\n' > "${bin}/qtpaths6"
chmod +x "${bin}"/*
run() {
    TEMPERANCE_IN_TERMINAL=1 TEMPERANCE_ROOT="${root}" PATH="${bin}:${PATH}" \
        bash "${package}/install-system.sh" </dev/null
}

# An install leaves this package's own files.
run >/dev/null
cmp "${package}/co.goodinput.temperance.so" \
    "${root}/usr/lib/qt6/plugins/plasma/applets/co.goodinput.temperance.so"

# A run whose copy never happens, over that earlier install, fails and says so.
printf 'newer plugin' > "${package}/co.goodinput.temperance.so"
printf '#!/bin/sh\nexit 0\n' > "${bin}/sudo"
if output="$(run 2>&1)"; then
    printf '%s\n' "installer: claimed success over an earlier install" >&2
    exit 1
fi
grep -q "not this package's" <<<"${output}"

# A build for the running Plasma and Qt installs without a warning; one for
# another still installs, and says the panel will show it needs an update.
printf '#!/bin/sh\nexec "$@"\n' > "${bin}/sudo"
printf 'ELF\0temperance-built-for plasma 6.7.4 qt 6.11.2\0' > "${package}/co.goodinput.temperance.so"
if run 2>&1 | grep -q "needs an update"; then
    printf '%s\n' "installer: warned about the Plasma and Qt it was built for" >&2
    exit 1
fi
printf 'ELF\0temperance-built-for plasma 6.8.0 qt 6.11.2\0' > "${package}/co.goodinput.temperance.so"
output="$(run 2>&1)"
grep -q "built for Plasma 6.8.0 and Qt 6.11.2" <<<"${output}"
grep -q "needs an update" <<<"${output}"
cmp "${package}/co.goodinput.temperance.so" \
    "${root}/usr/lib/qt6/plugins/plasma/applets/co.goodinput.temperance.so"

echo "Installer checks passed."
