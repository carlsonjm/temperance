#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 carlsonjm
# SPDX-License-Identifier: ISC
set -euo pipefail

repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_dir}/build"
release_dir="${repo_dir}/release"

cmake -S "${repo_dir}" -B "${build_dir}" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "${build_dir}"
ctest --test-dir "${build_dir}" --output-on-failure

version="$(sed -nE \
    's/^[[:space:]]*project\(temperance[[:space:]]+VERSION[[:space:]]+([^[:space:])]+).*/\1/p' \
    "${repo_dir}/CMakeLists.txt")"
architecture="$(uname -m)"
package_name="Temperance-${version}-${architecture}"
package_dir="${release_dir}/${package_name}"
archive_path="${release_dir}/${package_name}.tar.gz"

if [[ -z "${version}" || "${package_dir}" != "${release_dir}/Temperance-"* ]]; then
    printf '%s\n' "Unable to determine a safe release package path." >&2
    exit 1
fi

cmake -E rm -rf "${package_dir}"
install -d "${package_dir}"
install -m755 -s "${build_dir}/bin/plasma/applets/studio.warbler.temperance.so" \
    "${package_dir}/studio.warbler.temperance.so"
install -m644 "${repo_dir}/assets/studio.warbler.temperance.png" "${package_dir}/"
install -m755 "${repo_dir}/packaging/install-system.sh" \
    "${repo_dir}/packaging/uninstall-system.sh" "${package_dir}/"
install -m644 "${repo_dir}/packaging/README.md" "${repo_dir}/CHANGELOG.md" \
    "${repo_dir}/THIRD_PARTY_NOTICES.md" "${package_dir}/"
cmake -E copy_directory "${repo_dir}/LICENSES" "${package_dir}/LICENSES"

release_epoch="${SOURCE_DATE_EPOCH:-$(git -C "${repo_dir}" log -1 --format=%ct)}"
tar --sort=name --mtime="@${release_epoch}" --owner=0 --group=0 --numeric-owner \
    -C "${release_dir}" -czf "${archive_path}" "${package_name}"

printf 'Built %s\n' "${archive_path}"
