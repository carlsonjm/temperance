# Architecture

How Temperance works and why. Every input it answers is in `INPUT.md`, the split
with Ambient Tettegouche is `AMBIENT-BOUNDARY.md`, and planned work is
`ROADMAP.md`.

## What Temperance is

Temperance is a Status Bar for Plasma 6: notifications, weather, battery, Control
Center, an organized System Tray, and the time and date. It is one native
containment plugin that replaces the stock System Tray's presentation, not a
user-local QML-only plasmoid. Keep the root and native-plugin metadata identical,
keep the custom containment type, and rebuild for incompatible Qt or Plasma ABIs.
The release target is x86_64 Plasma 6.7.4, Qt 6.11.2 and KDE Frameworks 6.29 on
a horizontal Wayland panel.

The launcher, task manager, stock panel spacers and Ambient Tettegouche are
independent applets. Temperance may measure their geometry but never configures
or owns them. It reports what changed through notifications; Ambient owns what is
ongoing.

## Width

Adaptive mode measures from Temperance's right edge to the nearest visible
non-spacer applet on its left, leaving the configured gap. Plasma can move an
ancestor AppletContainer without changing a child root's local geometry, so the
watcher observes each relevant root and its ancestor chain, including
reparenting. Updates are event driven: no polling and no guessed target width.

Manual mode uses a fixed configured width. In both modes controls keep their
minimum widths, and the ticker clips, scrolls or drops optional content inside the
allocation. Temperance never writes spacer configuration or recenters the task
dock.

A container that paints furniture of its own, as Shuffle's band paints its dock,
is not in the applet list, so the measurement runs past it. Such a container holds
each component to its own side, and its clamp decides the width.

## Status icons

The bell, weather, Control Center, tray and battery are drawn a third larger than
the panel's small icons and set one 34 px step apart with no gaps, so they read as
one group; at the suite's usual 44 px they looked disconnected. Height makes up
the width: each icon's touch area runs the panel's full height, about 7 by 13 mm
on the Z13's band.

The row stays a 42 px surface because the popups are placed from it. The reach
above and below is a separate handler that acts only outside the row: a pointer's
press reaches both it and the control, and acting on both opened a page and closed
it again.

The bell and the wired connection's port are outlines Temperance draws. They are
kept to the size of the theme's filled marks beside them, 21 and 18 px in a
1.75 px line, because an outline reads larger than a filled mark in the same box.
The bell's count sits 5 px under its rim, in 13 px type.

The protected bell and tray are scaled, not redrawn. The network icon is the
theme's bold symbolic mark: Plasma's network widget adds `-symbolic` to its own
state names in a panel, so Temperance drops that ending and maps the name to the
freedesktop symbolic one.

## Notifications

### One store, one presenter

Temperance initializes `NotificationManager::Server::self()` and suppresses the
stock Notifications applet while it is loaded. The server is shared storage and
protocol ownership. Never start a second notification daemon, change D-Bus
ownership, clear history during a presenter change, or let two presenters race. A
stock-presenter option needs a readiness handshake: keep Temperance active until
the stock applet is complete, destroy and forget the stock applet before restoring
Temperance, and fall back to Temperance if stock loading fails.

### Routes follow meaning

Normal notifications, including actionable ones, use the ticker and history; a
transfer's end uses the history alone (`AMBIENT-BOUNDARY.md`).
Critical notifications and the bounded system-alert policy may use priority
banners, and a notification held for a banner does not also enter the ticker.
Keeping a banner for review moves it to history without replaying the ticker.
Stable notification identity stops playback restarting after model resets or
unrelated arrivals. Priority windows never expose zero-height Wayland geometry,
and banners keep touch-safe actions, bounded text, direct dismissal and
reduced-motion behavior.

Presentation never invents urgency, completion or an action the producer did not
supply: Temperance adds no Snooze, and a private NotificationManager check proves a
producer's own `snooze` action is kept and dispatched.

### History keeps the producer's meaning

Named actions come from paired `ActionNamesRole` and `ActionLabelsRole`; empty,
unpaired and reserved default entries are not drawn as pills. The default action
falls back to the application's desktop entry, and child controls take their own
pointer and key events.
Single dismissal uses the child's model index; group Clear, expansion and collapse
keep group semantics. Action targets stay at least 44 px high, and wrapped action
rows contribute their real height so controls stay inside the card.

## Clock and calendar

Temperance draws the time over an optional date at the Status Bar's right end, so
a Shuffle installation needs no separate clock applet. It uses the system UI font
or one you pick, in fixed cells that reserve the widest time and date the
locale produces, so it never moves the layout. The day period is lowercase, light
and smaller, on the time's baseline.

Each new minute is dealt onto the old like a card dropping in from above; a card
from the side crossed the a.m. The deal keeps its
own pace whatever Plasma's animation speed, since nobody waits on the clock and at
four times the default it could not be seen; only Instant, which counts as reduced
motion, makes it a fade. Shuffle Lock shows the same clock as a block at a size
the caller gives, a bold time over the long date set to the time's width, so lock
and bar are one clock with one minute animation.

The calendar is a small card in the corner, built on Plasma's month grid
(`org.kde.plasma.workspace.calendar`) and holiday plugin; a Keyboard-sized card
from the bottom edge is shelved past 1.0.

Events come from private iCal addresses you paste, as Google, iCloud and
Outlook give out, read with KCalendarCore every 10 minutes; a failed read keeps the
last good copy. KDE's full calendar service was rejected: a background service
with its own database on every install. Each address is kept in the widget's
settings in your home folder, and anyone holding it can read that
calendar. Linked calendars are read-only: writing would need a sign-in per service,
and Google reviews apps that write to calendars.

A new notification comes out from beside the bell and rests against it, rather
than crossing the gap to the dock, which on a wide display took the line far
from the bell it belongs to. A line too long for the ticker comes in until its
start reaches the far edge, then scrolls once while it rests. It stays up 20 ms
for each pixel of gap and line together, at least three seconds, then goes.

Today's next timed event rides the ticker rather than a line beside the time. It
reads in as a notification does and holds still with a dot in its calendar's color
until it is moved into the history, a notification takes the ticker, or it ends.
A color you pick wins over the feed's own, which only iCloud's carries.
The lead says when without a second clock: Now while it is under way, In and the
minutes within the hour of its start, otherwise its start in the clock's form. A
notification leads with its application in secondary white, and a line too long
for the ticker ends in an ellipsis while it stands still. Events and notifications
share the history, titled Notifications & events, today's events first; holidays
and all-day events stay in the calendar card.

## Control Center and tray

Session controls live in the Control Center header. Lock, Restart and Shut Down
stand in the row; Log Out and Switch User wait behind More. All are on by default
except Switch User, which is opt-in. Lock asks nothing, since it loses no work;
Log Out calls `org.kde.Shutdown` directly; Restart and Shut Down go through
`org.kde.LogoutPrompt`, so Plasma confirms them. Bluetooth pairing is the installed KDE wizard; Temperance does not
implement pairing.

Promoted tray entries become Control Center pills and leave the organized tray,
which keeps the Apps, Devices, System order and drops empty sections from its
spacing. Controls the machine cannot use are omitted: battery follows
authoritative power state, and performance profiles appear only when the local
helper is found.

## Popups

`PanelPopupAnchor.qml` places the popup, and its `panelGap` is the gap to the
panel; `Dialog.floating` only treats and clamps screen edges. The critical
notification host carries its own anchor correction, because its window type does
not inherit AppletPopup clearance. `ExpandedRepresentation.qml` owns the rounded
surface, padding and entrance motion, and `availablePopupHeight()` caps content
height. Never change padding, content translation or height limits to fix a
placement error; measure the final window, the painted panel edge and the anchor
first.

Every page sits on one margin line 16 px in from each edge, shared by the title,
the content's outer edges and the header's last control, with smaller gaps inside a
group than the 24 px between groups. Notification cards, action pills and Control
Center follow the calendar's spacing, weights and chrome. Height follows content,
capped by the screen.

## Visual rules

Temperance follows the suite's shared visual language and applies these rules
itself:

- Ghost White (`#F8F8FF`) is the primary foreground.
- Temperance's own action chrome comes from a pinned Lucide Static 1.46.0 subset
  vendored here. Application, provider, weather and tray icons stay with their
  sources.
- The animated bell, Lucide's Bell redrawn for its clapper and slash, the weather
  treatment and the custom tray work are protected, not icon-migration targets.
- Controls keep distinct resting, hover, pressed, focus and selected states.
- Motion is interruptible and follows the platform animation setting, except the
  clock's minute.
- Width removes optional information before it crushes spacing.

## Weather

Plasma's installed Weather data comes first. With the online fallback on,
Temperance may send the configured station coordinates or location name to
Open-Meteo for current weather. The setting stays visible and can be turned off;
no other network provider is added silently.

## Licensing, packaging and checks

Inherited KDE and libdbusmenu code keeps its SPDX identity and licence texts, and
vendored Lucide/Feather assets keep their ISC/MIT notice. Product metadata,
third-party notices, the icon manifest and installed assets stay in sync.

`packaging/build-release.sh` is the supported release path. A release holds the
native plugin and required artwork and matches the intended build output.
KCalendarCore is a build and install dependency; the installer checks for it and
restarts the panel, so an update needs no sign-out. A package identity change
makes people remove and re-add the widget, as 1.1.0 showed, so identity changes
only in the suite's coordinated identity work (Block 10b).

Automated checks cover source, notification, session, popup, clock, calendar and
real-panel geometry contracts. The real-panel fixture pairs Temperance with a
separately built Tettegouche plugin, so it is not part of the automatic run.
Physical tests remain required for compositor placement, real producers, touch
and optical alignment, and a control is tested by click as well as by touch.
