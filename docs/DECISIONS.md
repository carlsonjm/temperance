# Durable decisions

This ledger records current architectural choices and the invariants that explain
them. It is grouped by subsystem; implementation chronology belongs in Git and
dated validation belongs in `archive/`.

## Product and repository boundary

### Temperance is a native Plasma containment

Temperance embeds a System Tray host, notification presentation, Control Center,
weather, and status rail in one native plugin. It is not a user-local QML-only
plasmoid. Keep the root and native-plugin metadata identical, preserve the custom
containment type, and rebuild for incompatible Qt or Plasma ABIs.

The launcher, task manager, clock, stock panel spacers, and Ambient Tette are
independent applets. Temperance may measure their geometry but must not configure
or own them.

### Live activity stays in Tette

Temperance communicates transitions through notifications. Ambient Tette owns
ongoing media, transfers, progress, and live actions. The full contract is in
`AMBIENT-BOUNDARY.md`.

## Panel sizing and geometry

### Adaptive width follows the actual scene-space allocation

Adaptive mode measures from Temperance's allocated right edge to the nearest
visible non-spacer applet on its left, leaving the configured inter-applet gap.
Plasma may move ancestor AppletContainers without changing a child root's local
geometry, so the watcher observes each relevant root and its ancestor chain,
including reparenting. Updates stay event driven; do not replace them with polling
or a guessed target width.

Manual mode uses `compactWidth`. In both modes, controls keep their minimum
widths and the ticker clips, scrolls, or reduces optional content inside the
allocation. Temperance does not write spacer configuration or recenter the task
dock.

### Popup placement and content layout have separate owners

`PanelPopupAnchor.qml` moves the dialog's visual parent away from the panel. Its
`panelGap` is the panel-side placement control. `Dialog.floating` supplies
screen-edge treatment and clamping; it is not the popup-to-panel gap. The critical
notification host has an additional anchor correction because its window type does
not inherit AppletPopup clearance.

`ExpandedRepresentation.qml` owns the rounded surface, internal padding, and
entrance motion. `availablePopupHeight()` limits content height. Do not adjust
internal padding, content translation, or height limits to fix an external
placement error; measure the final window, painted panel edge, and anchor first.

## Notifications

### One process-wide notification store, one active presenter

Temperance initializes `NotificationManager::Server::self()` and suppresses the
stock Notifications applet while Temperance is loaded. The server is shared
storage and protocol ownership. Do not start a second notification daemon, change
D-Bus ownership, clear history during a presenter change, or let two presenters
race.

A future stock-presenter option requires a readiness handshake: keep Temperance
active until the stock applet is complete, destroy and forget the stock applet
before restoring Temperance, and fall back to Temperance if stock loading fails.
That work is deferred until the roadmap explicitly activates it.

### Routes are based on notification meaning

Normal notifications, including actionable ones, use the ticker and history.
Critical notifications and the bounded system-alert policy may use priority
banners. A notification reserved for a banner does not simultaneously enter the
ticker. Keeping a banner for review moves attention to history without replaying
the ticker route. Stable notification identity prevents automatic playback from
restarting after model resets or unrelated arrivals.

Priority windows must never expose zero-height Wayland geometry. Banner content
retains touch-safe actions, bounded text, direct dismissal, and reduced-motion
behavior. Presentation must not invent urgency, completion, or an action the
producer did not supply.

### History preserves producer semantics

Named actions come from paired `ActionNamesRole` and `ActionLabelsRole`; empty,
unpaired, and reserved default entries are not rendered as named pills. A named
pill invokes only its named action. The card body invokes the default action, then
may fall back to the source application's desktop entry. Child controls consume
their own pointer and key events.

Individual dismissal uses the child's model index. Group Clear, expansion, and
collapse retain group semantics. Formatted bodies may expand in place. Action
targets remain at least 44 px high even when their visible pill is smaller, and
wrapped action rows must contribute their real height so controls stay inside the
card.

## Control Center and tray

Session controls live in the popup header. Log Out, Restart, and Shut Down are
enabled by default; Switch User is opt-in. Log Out is direct, while restart and
shutdown retain system confirmation. Bluetooth launches the installed native
pairing wizard; Temperance does not implement pairing.

Promoted tray applets become Control Center pills and leave the organized tray.
The tray keeps the Apps, Devices, System order and removes empty sections from the
spacing hierarchy. Battery and performance controls remain capability gated:
battery uses authoritative power state, and performance profiles appear only when
the local helper is discovered.

## Visual and interaction language

The live visual contract is `ITASCA-VISUAL-LANGUAGE.md`. Ghost White
(`#F8F8FF`) is the primary suite foreground, and Lucide Static 1.46.0 is the
pinned source for suite-owned action chrome. Application, provider, weather, and
tray identities remain source-owned.

Temperance's animated bell, weather treatment, and custom tray work are protected
exceptions. Their geometry and behavior are not generic icon-migration targets.
Controls retain distinct resting, hover, pressed, focus, and selected states;
motion must remain interruptible and respect the platform animation setting.

## Weather and external data

Installed Plasma Weather data is preferred. When online fallback is enabled,
Temperance may send the configured station coordinates or location name to
Open-Meteo for current weather. The setting must remain visible and disableable;
do not silently add another network provider.

## Licensing, packaging, and validation

Inherited KDE and libdbusmenu code keeps its SPDX identity and license texts.
Vendored Lucide/Feather assets keep their ISC/MIT notice. Product metadata,
third-party notices, the icon manifest, and installed assets must remain in sync.

`packaging/build-release.sh` is the supported release path. A release artifact
must contain the native plugin and required artwork and must match the intended
build output. Automated tests protect source, notification, session, popup, and
panel geometry contracts; physical tests remain required for compositor placement,
real producers, touch behavior, and optical alignment.

## Measure to a published edge rather than inferring one

Adaptive width works out how much room this applet has by measuring from the
nearest applet on its left. That is right on a panel, where everything
occupying the row is an applet, and wrong where a container paints furniture
of its own: painted furniture is not in the applet list, so the measurement
runs past it and claims room that was never free.

The applet keeps every measurement it makes about itself --- its content, its
floor, the output it is on. It stops inferring the one number it cannot see,
and reads it where a container offers it.

The read is discovered at run time and never sampled on a timer: the change
signal is what it acts on. An unknown major version is refused rather than
guessed at, and a container that publishes nothing, or none at all, leaves
every measurement exactly as it is on a plain panel. That is what keeps this
applet independent of any particular container rather than adapted to one.

Being clamped by a container is not a substitute for this and does not replace
it. A clamp stops a wrong number mattering in that container; this stops the
wrong number being computed anywhere.
