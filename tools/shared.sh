#!/usr/bin/env bash
# src/shared is code Temperance runs as Tettegouche's Search does: whether
# Shuffle is installed, Configure opening Shuffle Settings, and the room a
# widget has along its panel. It changes in Tettegouche, and Temperance keeps
# a byte-for-byte copy. This records that copy, file by file, so the two
# widgets cannot drift apart:
#
#   tools/shared.sh check       each file is still the copy recorded, and no
#                               other is there (no network; run by verify.sh)
#   tools/shared.sh changes REF what Tettegouche changed in these files since
#                               the recorded commit, up to REF, such as next
#   tools/shared.sh take REF    copies Tettegouche's files at REF here and
#                               prints the commit and hashes to record below
#
# A change to these files is made in Tettegouche first, then taken here.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
shared_repository=https://github.com/carlsonjm/tettegouche.git
shared_commit=ab9b99a81fe6761a7f347e84001e4c4007810ed9

# sha256 and path, the same in both repositories.
files() {
    cat <<'LIST'
962f5cc8f140ae110eeeb174d0a344a61911df03c4f6e635610fc08643433d9b src/shared/PanelRoom.h
aac083fbf45c9e520b94467aeb5078c01c6474ded95e1060659af7ca5cb7d15f src/shared/SuitePresence.h
f7d1fa4dc05566ef4365e2da5f3e74d6680f2f2859ba94be38327bdb907c4f00 src/shared/SuiteSettings.h
LIST
}

fetch() {
    local ref="$1" into="$2"
    git init --quiet "${into}"
    git -C "${into}" fetch --quiet --depth 1 "${shared_repository}" "${ref}"
    git -C "${into}" rev-parse FETCH_HEAD
}

case "${1:-check}" in
    check)
        failed=0
        while read -r sum path; do
            if [[ ! -f "${root}/${path}" || "$(sha256sum < "${root}/${path}" | cut -c1-64)" != "${sum}" ]]; then
                printf 'shared: %s is no longer Tettegouche %s; make the change there and take it\n' \
                    "${path}" "${shared_commit:0:7}" >&2
                failed=1
            fi
        done < <(files)
        while read -r path; do
            if ! files | cut -d' ' -f2 | grep -qxF "${path}"; then
                printf 'shared: %s is not one of the shared files recorded here\n' "${path}" >&2
                failed=1
            fi
        done < <(cd "${root}" && find src/shared -type f | sort)
        (( failed == 0 )) || exit 1
        echo "Shared code matches Tettegouche ${shared_commit:0:7}."
        ;;
    changes)
        ref="${2:?name the Tettegouche branch or commit to compare with, such as next}"
        work="$(mktemp -d)"; trap 'rm -rf -- "${work}"' EXIT
        fetch "${shared_commit}" "${work}/from" > /dev/null
        to="$(fetch "${ref}" "${work}/to")"
        git -C "${work}/to" fetch --quiet --depth 1 "${shared_repository}" "${shared_commit}"
        git -C "${work}/to" diff "${shared_commit}" "${to}" -- src/shared
        ;;
    take)
        ref="${2:?name the Tettegouche branch or commit to take, such as next}"
        work="$(mktemp -d)"; trap 'rm -rf -- "${work}"' EXIT
        commit="$(fetch "${ref}" "${work}/tettegouche")"
        git -C "${work}/tettegouche" checkout --quiet FETCH_HEAD -- src/shared
        rm -rf -- "${root}/src/shared"
        cp -R "${work}/tettegouche/src/shared" "${root}/src/shared"
        echo "shared_commit=${commit}"
        (cd "${root}" && find src/shared -type f | sort | xargs sha256sum | sed 's/  / /')
        ;;
    *)
        echo "usage: tools/shared.sh [check | changes REF | take REF]" >&2
        exit 2
        ;;
esac
