# Notification presenter handoff

Status: source investigation complete; implementation intentionally deferred from
the banner-layout slice.

## Ownership finding

Temperance and Plasma's stock Notifications applet run inside `plasmashell` and
share `NotificationManager::Server::self()`. The setting must therefore switch
the visual presenter, not unregister, replace, or restart the
`org.freedesktop.Notifications` service.

Plasma's Notifications `Globals.qml` keeps one process-wide popup model. Its
model limit is zero when no notification plasmoid has been adopted. The stock
applet calls `Globals.adopt(root)` when its QML completes and
`Globals.forget(root)` on destruction. Creating and removing the stock applet is
therefore the supported presenter lifecycle; deleting the applet does not leave
a hidden popup presenter active.

## Prepared implementation

1. Keep the unconditional `NotificationManager::Server::self().init()` in
   `src/systemtray.cpp`. It is shared storage and protocol ownership, not
   Temperance-specific presentation.
2. Replace the unconditional Notifications rejection in
   `SystemTray::startApplet()` with an explicit requested-presenter policy.
3. Add a native presenter state and readiness signal. When stock presentation is
   requested, create `org.kde.plasma.notifications`, wait until its applet item
   has completed, then report stock-ready. Temperance continues presenting until
   that signal, preventing a handoff gap.
4. Bind the QML rail, bell, priority banners, and history front door to the
   native presenter state rather than directly racing the configuration value.
5. When Temperance presentation is requested again, destroy the stock applet
   synchronously, verify it has been forgotten, and only then enable
   Temperance's visual routes. Notification history remains shared throughout.
6. Keep the stock applet out of Temperance's custom front-door model while
   Temperance owns presentation. When stock owns presentation, expose its normal
   compact/history entry rather than recreating its actions.

Do not mutate D-Bus ownership, run a second notification daemon, clear history,
or suppress notifications at the server as part of this switch.

## Acceptance matrix

- Start `plasmashell` with either setting and receive exactly one popup route.
- Toggle both directions with no lost or duplicate notification.
- Toggle while a notification is visible and while several are queued.
- Verify critical, normal, action, reply, resident, and job notifications.
- Verify Do Not Disturb, history, default actions, dismissal, and unread counts.
- Verify a failed stock-applet load leaves Temperance presentation active and
  surfaces a diagnostic instead of silently dropping alerts.
- Verify repeated rapid toggles settle on the final requested owner.

This handoff should remain its own reviewed ownership packet. Banner geometry
does not depend on it.
