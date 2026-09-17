#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="$(mktemp -d "${TMPDIR:-/tmp}/temperance-verify.XXXXXX")"
trap 'rm -rf -- "${build_dir}"' EXIT

python3 "${project_root}/tests/verify-docs.py"
cmake -S "${project_root}" -B "${build_dir}" -DBUILD_TESTING=ON
cmake --build "${build_dir}" -j2
ctest --test-dir "${build_dir}" --output-on-failure

echo "Temperance verification passed."
