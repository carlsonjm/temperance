# Notification history actions — September 15, 2026

A-Team source-only correction from protected design baseline **e9800f7** on
`a/notification-history-actions`, worktree `../.worktrees/temperance-history-actions`.
B owns the separate visual migration and will integrate this correction.

## Verified API and behavior

Installed `plasma-workspace` / `libplasma`: **6.7.5-1.1**. The local
`notificationmanager/notifications.h` and
`org/kde/notificationmanager/notificationmanager.qmltypes` expose:

- `ActionNamesRole` / `ActionLabelsRole` → `actionNames` / `actionLabels`.
  Header documents that default/settings actions are excluded.
- `invokeAction(QModelIndex, QString)` for named producer actions.
- `invokeDefaultAction(QModelIndex)` for the card's default action.
- `close(QModelIndex)` for individual dismissal, or group dismissal when the index
  belongs to a group. `IsGroupExpandedRole` remains writable through `setData`.

The baseline history page did not consume named-action roles and exposed no
individual dismiss button. The correction adds both to non-group cards.

`NotificationHistoryPage.qml` now consumes paired producer names/labels, excludes
`default` defensively and ignores unpaired/empty entries. Producer labels and IDs
are otherwise preserved. Text pills wrap within available width, have at least
44 px height, an 8 px gap, Ghost White copy/focus outline, and accessible names.
Dismiss passes the current child row's index; it never substitutes a group index.
No invented Open/default pill is added. Whole-card default action or application
fallback remains, as do Show more/less, group clear and group expansion.

Pills accept mouse/touch separately from the earlier card MouseArea. Space uses
native ToolButton behavior; explicit Enter/Return handlers prevent propagation to
the card's default-open handler. A test exposed the missing Return dispatch before
this handler was added. No icon assets, visual-kit implementation, popup width,
height cap, card surface radius or panel/rail geometry are changed.

## Popup-to-dock spacing: owner identified, correction not established

The supplied 465×465 screenshot shows the notification surface but **does not
include the dock edge**. It cannot establish a measured external gap or the popup's
final window coordinates.

Current placement boundary:

1. `qml/main.qml`: `popupAnchor` is attached to the compact rail; its bottom-edge
   offset is already `y: -10` (top edge +10). `dialog` is `PlasmaCore.Dialog`, type
   `AppletPopup`, `visualParent: popupAnchor`, `floating: 10`, `NoBackground`.
2. Installed `PlasmaQuick/dialog.h` documents `floating` as a distance from screen
   borders; it is **not a dedicated popup-to-painted-panel gap property**.
   `visualParent`, `location`, `floating` and `popupPosition` belong to Dialog.
3. Upstream [Dialog::popupPosition](https://github.com/KDE/libplasma/blob/master/src/plasmaquick/dialog.cpp)
   maps the anchor to global coordinates, chooses the adjacent edge and clamps to
   available screen geometry inset by `floating`. This upstream source corroborates
   the API ownership; it is not claimed to be byte-identical to the installed build.
4. `ExpandedRepresentation.qml` owns the rounded content surface and internal
   safety spacing. Its entrance translation settles to zero. Moving only that
   content would not reliably move the external window/input region together.
   `SystemTray::availablePopupHeight` only limits content height; it does not place
   the popup.

**No spacing patch:** retain the existing 10 px intent. The main.qml comment that
floating supplies panel clearance should not be treated as an API guarantee.
A safe external correction needs the final popup rectangle, the dock's painted
edge and panel window/anchor rectangles on the failing configuration. Without
those, adding/subtracting another 10 px could double the offset or be absorbed by
screen clamping. No internal padding, blind shrink, role change or window-position
fight is introduced. B should keep this boundary intact during integration; this
commit does not claim the visible spacing issue is resolved.

## Validation

- Production applet and notification interaction test built successfully in
  `/tmp/temperance-history-actions-build`, including compiled QML.
- Focused tests use a typed QAbstractListModel fixture with the installed enum roles,
  QStringList data and QModelIndex API arguments. This verifies UI dispatch, not
  a claim that a real notification producer has accepted an action.
- Four existing mouse/touch standalone/group-child expansion/default/fallback cases
  now also verify individual dismiss. Two added mouse/touch cases verify named
  actions, exclusion of default/unlabelled entries, individual child dismissal,
  Space/Return separation, whole-card default, and group clear/collapse/expand.
- Final direct private-bus/offscreen run: **8 passed, 0 failed, 0 skipped** (six cases
  plus setup/cleanup), log `/tmp/temperance-history-focused-final.log`.
- Source sanity and `git diff --check` passed. Existing localization/deprecation and
  headless platform warnings remain; no QML binding/type errors in the focused run.
- Initial inherited-session test timed out. The existing isolated harness passed
  once; a later sandboxed wrapper retry could not create its D-Bus socket. No further
  approval wait was opened. The already-authorized direct `dbus-run-session` command
  with the repository's no-service-activation bus config ran the final test above.
  CTest now uses the existing isolated harness for reproducibility outside that
  sandbox wrapper limitation.

No installation, live notification mutation, effect toggle, service restart or
push. No post-install or real-producer physical pass is claimed.

## B integration and J physical checks

Cherry-pick the bounded commit onto B's visual branch. Likely overlap is only the
history delegate; preserve B's existing visual changes while retaining the new
required roles, paired-action filter, action/dismiss dispatch, and pointer/key
separation. The local NotificationActionPill may adopt B's shared pill styling,
but preserve its semantics and 44 px target. No change to `main.qml` or icon assets
is part of this patch. Keep the test linkage to LibNotificationManager and the
private test harness.

After B's integration and separately authorized install:

1. Receive a normal notification with two real producer actions and a default
   action. Each pill invokes only its named action; card body invokes only default.
   Test pointer, touch, keyboard Space/Enter and screen-reader naming.
2. Dismiss one standalone alert and one child of a group; siblings remain. Group
   Clear still dismisses the group; collapse/expand and Show more/less still work.
3. Check long producer labels wrap without escaping the card; scroll a full history
   and verify popup width, maximum height and rounded surface remain intact.
4. For spacing, capture both popup and dock after entrance settles, on the failing
   layout. Measure the two visible edges and corresponding window/anchor rectangles
   before selecting any external-placement correction. The target remains 10 px.
