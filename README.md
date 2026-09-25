<p align="center">
  <img src="assets/studio.warbler.temperance.png" width="180" alt="Temperance">
</p>

# Temperance

Temperance is a Status Bar for KDE Plasma 6. It keeps system state and recent
changes together in one panel widget: notifications, weather, battery, common
controls, the System Tray, and the time and date.

Temperance replaces Plasma's stock System Tray presentation and brings its own
clock, which can be turned off where a panel keeps Plasma's. Your launcher, task
manager, and other panel widgets remain independent.

## Status Bar

The compact panel view can show:

- Recent notification activity
- Current weather
- Battery state
- Volume and brightness
- Network and other System Tray entries
- The time over the date, at the right end, in the system font or one you choose

Temperance adjusts its width to the space available in the panel. Optional modules
can be turned off, and the remaining sections share the available space.

## Notifications

New notifications appear briefly in the Status Bar and remain available in recent
history. Notifications from the same application can be grouped and expanded.

When an application supplies actions such as reply, dismiss, or snooze, Temperance
keeps those actions with the notification. It does not invent actions that the
application did not provide.

Critical and selected system notifications can appear as larger banners. Ordinary
notifications stay in the quieter ticker and history flow.

## Control Center

Control Center provides quick access to volume, brightness, connectivity, battery,
and the performance controls supported by the current system.

Unavailable controls are omitted. Temperance does not show a control that the
machine cannot use.

## System Tray

The organized System Tray keeps application and device entries in one place. Each
entry can be shown in the panel, placed in the tray popup, or disabled through the
widget settings.

Control Center and System Tray remain available even when optional notification or
weather sections are turned off.

## Configure

Right-click the widget and choose **Configure Temperance**.

- **Tray Entries** controls which native tray entries appear in the panel, in the
  popup, or nowhere.
- **Appearance** controls the highlight color, automatic or fixed width, optional
  notification and weather sections, temperature unit, weather application,
  important-alert behavior, popup pin, and Control Center entries.

When online weather fallback is enabled, Temperance sends the configured station
coordinates or location name to Open-Meteo for the current temperature. Disable
the fallback to keep weather data local to Plasma's installed Weather widget.

## Compatibility

The 1.1.0 binary bundle targets x86_64 Plasma 6.7.4, Qt 6.11.2, and KDE Frameworks
6.29 on a horizontal Wayland panel.

Temperance includes a native C++ plugin and must be rebuilt after incompatible Qt
or Plasma upgrades. Building from source requires Qt 6.9 or newer and KDE
Frameworks 6.20 or newer.

Because the package identity changed in 1.1.0, remove the previous package and add
Temperance to the panel as a new widget after installing this release.

## Build from source

Install a C++20 compiler, CMake, Ninja, ECM, Plasma and PlasmaQuick development
files, Qt 6 Core/DBus/Gui/Quick/Widgets development files, KDE Frameworks 6
development files, PulseAudioQt, and NotificationManager development files.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
```

Create the tested installable release archive with:

```sh
./packaging/build-release.sh
```

Native Plasma plugins are installed through a compatible native package. They are
not ordinary user-local `.plasmoid` archives.

## Development

Read `AGENTS.md` before working in this repository. The documentation index is in
`docs/README.md`.

- `docs/CURRENT_STATE.md` describes current behavior and limitations.
- `docs/NEXT-ROADMAP.md` is the execution plan.
- `docs/DECISIONS.md` records durable architecture and invariants.

Run the repository checks before proposing a build:

```bash
./verify.sh
```

## Architecture and attribution

The tray host is derived from KDE Plasma Workspace 6.7.4. Presentation code lives
in `qml/`; the inherited native tray implementation lives in `src/` and
`libdbusmenuqt/`.

See the SPDX headers, [LICENSES](LICENSES), and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for licensing and required
third-party attribution.

The project names and marks are not covered by that licence. See
[TRADEMARKS.md](TRADEMARKS.md).
