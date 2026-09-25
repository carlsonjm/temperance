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

The launcher, task manager, stock panel spacers, and Ambient Tette are
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

## Clock and calendar

### The clock is Temperance's, and its minute deals like a card

Settled by J on 24 September. Temperance shows the time and date at the right
end of the Status Bar, and a Shuffle installation carries no separate clock
applet. The clock uses the system UI font unless the person chooses another, and
its digits never move the layout. The time is Ghost White and the date
secondary. Each new minute arrives like a card dealt onto a stack; under reduced
motion it fades. A tap opens the calendar as a small card in the corner, where
people expect it, with holidays and the person's own events. Chosen over the
calendar rising from the bottom edge as a card the Keyboard's size, which J
shelved past 1.0 together with a timeline.

The deal keeps its own pace whatever Plasma's animation speed (J, 25 September).
That setting shortens what a person waits on, and nobody waits on the clock; at
four times the default speed the deal lasted 50 ms and could not be seen. Instant
still counts as reduced motion. The card drops in from above, nearer the eye, while
the old glyph darkens and sinks; a small drop read as a bump, and a card from the
side crossed the a.m. and failed. The day period reads a.m. and p.m., lowercase
and light, smaller than the time and on its baseline, beside a time in the
regular weight.

Shuffle Lock shows this clock too, as a block of its own rather than the bar's
two lines scaled up, which blurred (J, 25 September): a bold time over the long
date with a bold weekday, the date sized to the time's width so the two lines
make one shape. It is chosen by the caller and leaves the bar unchanged, so the
lock and the bar stay one clock with one minute animation.

Events come from a private calendar link the person pastes once, which Google,
iCloud and Outlook all give out, read with KDE's calendar library (J, 24
September). Read-only and a few minutes behind, and chosen over KDE's full
calendar service, a background service with its own database on every install.
Several calendars can be linked (J, 25 September). Each address is kept in the
widget's settings in the person's home folder, and anyone holding it can read
that calendar. The calendar stays read-only (J, 25 September): events are added
in each service's own app. Writing would need a sign-in per service, and Google
reviews apps that write to calendars.

Today's next timed event rides the notification ticker (J, 25 September),
chosen over a line beside the time. It reads in from the right as a notification
does, then holds still instead of passing, with a dot at its end to say so,
until the ticker's arrow lands it in the history or a notification needs the
ticker. Events and notifications share the history, titled Notifications &
events. The dot takes the calendar's colour: the one the person picks for it
in the Calendar settings (J, 25 September), else the feed's, which iCloud's
carries and Google's and Outlook's do not. Holidays and all-day events stay in
the calendar card.

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

## Measure what can be seen, and let a container's clamp decide the rest

Adaptive width works out how much room this applet has by measuring from the
nearest applet on its left. That is right on a panel, where everything
occupying the row is an applet. A container that paints furniture of its own,
as Shuffle's band paints its dock, is different: the furniture is not in the
applet list, so the measurement runs past it. Such a container holds each
component to its own side, and that clamp decides the width.

Temperance also read the band's published dock edge for a time. It was retired
on 25 September (audit 20): on the band its result matched the clamp to two
pixels, no other container publishes one, and it tied a component to one
product's bus. The band still publishes the edge for the Keyboard's handle.
