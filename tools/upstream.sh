#!/usr/bin/env bash
# The tray host, the libdbusmenu copy and the tray's QML are derived from KDE
# Plasma Workspace. This records the upstream release they come from, file by
# file, so a new Plasma's fixes can be carried over as a diff:
#
#   tools/upstream.sh check        files kept identical to upstream still are
#                                  (no network; run by verify.sh)
#   tools/upstream.sh local        how each changed file differs from upstream
#   tools/upstream.sh changes TAG  what upstream changed in these files between
#                                  the recorded release and TAG, such as v6.8.0
#
# A file marked same is a byte-for-byte copy: carry upstream's changes over
# as they are. Temperance's own work lives in separate files, so the changed
# ones stay small. After a rebase, record the new release and hashes here.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
upstream_repository=https://github.com/KDE/plasma-workspace.git
upstream_tag=v6.7.4
upstream_commit=fd05f4c88ab093aee23ce137bf6f2412437c9bba

# state, upstream sha256 for a copy kept the same, local path, upstream path.
files() {
    cat <<'LIST'
same 331fe93eaabb13accaef18325719d1303d6ccb4677de7d10aa307365d3cffc68 src/dbusserviceobserver.cpp applets/systemtray/dbusserviceobserver.cpp
same 05f00e4eee08de56d1e8746ea373501180aeab8a01c802827de3a2febd54c22c src/dbusserviceobserver.h applets/systemtray/dbusserviceobserver.h
same 33f6847601d33a8ca71e683b82177caae3a171605e6ff8b0e0d0a8aca14dbcef src/plasmoidregistry.cpp applets/systemtray/plasmoidregistry.cpp
same d9b45fcf00d777b6ec6c2da6b267df8e6a179844258fe22456806599c2a4cd83 src/plasmoidregistry.h applets/systemtray/plasmoidregistry.h
same 4186a287bc2dab512fc3de4b882c8d9fb577badf915055dce9b33d6eafadf344 src/sortedsystemtraymodel.cpp applets/systemtray/sortedsystemtraymodel.cpp
same 4933091b3034d4c9a105267c587fb8c25a7bbb258ad81c958d922cde1e6fd90a src/sortedsystemtraymodel.h applets/systemtray/sortedsystemtraymodel.h
same 55bb554fceef7526d29f5fbc2642edc2f567cc9402b9dc7c524bb10e143426df src/statusnotifieritemhost.cpp applets/systemtray/statusnotifieritemhost.cpp
same 2ccc8d0cc209711e25dc540bd1375c985fed00d7a234bc37166b7cfb75dc259a src/statusnotifieritemhost.h applets/systemtray/statusnotifieritemhost.h
same 620b9bdb6f01929ac76d85f14b49902b17c42f8bfda5c8afb81085c02fd0b7b2 src/statusnotifieritemsource.cpp applets/systemtray/statusnotifieritemsource.cpp
same 03d71674f6be430a77a1d61cc404da2637967ae5fbb5d20d693633a046750fa3 src/statusnotifieritemsource.h applets/systemtray/statusnotifieritemsource.h
same 68e44c2520ddbf7a4b8a32e46c07e312cf0949f183e4a6267c56523a763961e3 src/systemtraytypes.cpp applets/systemtray/systemtraytypes.cpp
same e4f64f87bd730541dcfcc3818da251b4b07b09ee5de080dc0ae1380513f70dda src/systemtraytypes.h applets/systemtray/systemtraytypes.h
changed - src/systemtray.cpp applets/systemtray/systemtray.cpp
changed - src/systemtray.h applets/systemtray/systemtray.h
changed - src/systemtraymodel.cpp applets/systemtray/systemtraymodel.cpp
changed - src/systemtraymodel.h applets/systemtray/systemtraymodel.h
changed - src/systemtraysettings.cpp applets/systemtray/systemtraysettings.cpp
changed - src/systemtraysettings.h applets/systemtray/systemtraysettings.h
same 534f7da504fda77b31f3464eadf2b37fee2ec26712f5ff5d7fcf3c775ea26f25 libdbusmenuqt/com.canonical.dbusmenu.xml libdbusmenuqt/com.canonical.dbusmenu.xml
same 7d471dc499de775de7069f7f6f2c0d69d3e42e26d8dc4281cdd19c5e1d6ef547 libdbusmenuqt/dbusmenuimporter.cpp libdbusmenuqt/dbusmenuimporter.cpp
same 04eaf8f2e9c5709aa5c42a107a69489f12b0ce238a8a28565552525a882a506c libdbusmenuqt/dbusmenuimporter.h libdbusmenuqt/dbusmenuimporter.h
same 7b5c8eb52cc53dc666549a18bee8bfedd6a946d0f102b52631c4c8c79492b341 libdbusmenuqt/dbusmenushortcut_p.cpp libdbusmenuqt/dbusmenushortcut_p.cpp
same aa8cfab9c2e7676b8f3718c06a74e8c2b744f51948fd12e3effbf538c6759967 libdbusmenuqt/dbusmenushortcut_p.h libdbusmenuqt/dbusmenushortcut_p.h
same 9c3303f8f7ac746ecc6841882bc2acdc8f12f6762ffd4c5ad37326a813ff69e4 libdbusmenuqt/dbusmenutypes_p.cpp libdbusmenuqt/dbusmenutypes_p.cpp
same abc5f00f427df2ee3ce5346cd6896f69084a08cccf287f936cb91ccfaf14e511 libdbusmenuqt/dbusmenutypes_p.h libdbusmenuqt/dbusmenutypes_p.h
same 9dfb9c9d2ad66430691e1602746db492e8d238326289f59b63f72ca7ae03bc40 libdbusmenuqt/utils.cpp libdbusmenuqt/utils.cpp
same 1bb9e3f4f88ab27c64e6385420e9a118e449c86e93e591c45393e48ec72346c5 libdbusmenuqt/utils_p.h libdbusmenuqt/utils_p.h
changed - qml/AbstractItem.qml applets/systemtray/qml/AbstractItem.qml
changed - qml/BackgroundAppItem.qml applets/systemtray/qml/BackgroundAppItem.qml
changed - qml/CompactApplet.qml applets/systemtray/qml/CompactApplet.qml
same 2752da642d5ddaa2045a647d3ae119d5329bb9bd6897bf0b988afed8f53cb1b0 qml/ConfigGeneral.qml applets/systemtray/qml/ConfigGeneral.qml
same 85e43b4d7e146e7cc9de07eb86b6a32f8dc08578211f819b5f2bbe972d65adbe qml/CurrentItemHighLight.qml applets/systemtray/qml/CurrentItemHighLight.qml
changed - qml/ExpandedRepresentation.qml applets/systemtray/qml/ExpandedRepresentation.qml
same d8fe2b824cc37f614c02e737b5decfcf3b4ffa870821539004b16cb4aedaa7eb qml/ExpanderArrow.qml applets/systemtray/qml/ExpanderArrow.qml
same 13de6193471c669ca6808c4c07938c44550c0160f9f6b8fb92baded759f35939 qml/HiddenItemsView.qml applets/systemtray/qml/HiddenItemsView.qml
changed - qml/ItemLoader.qml applets/systemtray/qml/ItemLoader.qml
changed - qml/PlasmoidItem.qml applets/systemtray/qml/PlasmoidItem.qml
same bef9f70f1906d3f779efd3bcf9b65500a613cdb202156a623c9f38490e466466 qml/PlasmoidPopupsContainer.qml applets/systemtray/qml/PlasmoidPopupsContainer.qml
same ceb3c4fdc75414970dd3395fbbe83dc1717ed4adf71e5ce1f104e94fd86f5774 qml/PulseAnimation.qml applets/systemtray/qml/PulseAnimation.qml
changed - qml/StatusNotifierItem.qml applets/systemtray/qml/StatusNotifierItem.qml
changed - qml/SystemTrayState.qml applets/systemtray/qml/SystemTrayState.qml
changed - qml/config.qml applets/systemtray/qml/config.qml
changed - qml/main.qml applets/systemtray/qml/main.qml
LIST
}

fetch() {
    local tag="$1" into="$2"
    git clone --quiet --depth 1 --branch "${tag}" --filter=blob:none --sparse \
        "${upstream_repository}" "${into}" 2>/dev/null
    git -C "${into}" sparse-checkout set applets/systemtray libdbusmenuqt
}

case "${1:-check}" in
    check)
        failed=0
        while read -r state sum local _; do
            [[ "${state}" == same ]] || continue
            if [[ "$(sha256sum < "${root}/${local}" | cut -c1-64)" != "${sum}" ]]; then
                printf 'upstream: %s is no longer %s %s; move the change into a file of its own, or mark it changed\n' \
                    "${local}" "Plasma Workspace" "${upstream_tag}" >&2
                failed=1
            fi
        done < <(files)
        (( failed == 0 )) || exit 1
        echo "Upstream copies match Plasma Workspace ${upstream_tag}."
        ;;
    local)
        work="$(mktemp -d)"; trap 'rm -rf -- "${work}"' EXIT
        fetch "${upstream_tag}" "${work}/upstream"
        [[ "$(git -C "${work}/upstream" rev-parse HEAD)" == "${upstream_commit}" ]] \
            || { echo "upstream: ${upstream_tag} no longer names ${upstream_commit}" >&2; exit 1; }
        while read -r state _ local remote; do
            [[ "${state}" == changed ]] || continue
            diff -u --label "upstream/${remote}" --label "${local}" \
                "${work}/upstream/${remote}" "${root}/${local}" || true
        done < <(files)
        ;;
    changes)
        tag="${2:?name the upstream release to compare with, such as v6.8.0}"
        work="$(mktemp -d)"; trap 'rm -rf -- "${work}"' EXIT
        fetch "${upstream_tag}" "${work}/from"
        fetch "${tag}" "${work}/to"
        while read -r _ _ local remote; do
            diff -u --label "${upstream_tag}/${remote} (${local})" --label "${tag}/${remote}" \
                "${work}/from/${remote}" "${work}/to/${remote}" 2>/dev/null || true
        done < <(files)
        ;;
    *)
        echo "usage: tools/upstream.sh [check | local | changes TAG]" >&2
        exit 2
        ;;
esac
