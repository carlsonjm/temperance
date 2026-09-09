# Temperance

Temperance is a Plasma 6 panel widget that combines a quiet notification rail,
a compact Control Center, and an organized System Tray. It replaces only the
stock System Tray; the launcher, task manager, and clock remain independent
panel widgets.

A Warbler Studio project. Under the leaves.

## Highlights

- Animated notification intake, compact paging, and app-grouped history
- Weather and battery information without a second status row
- Volume, brightness, connectivity, battery, and capability-detected performance controls
- Native tray visibility settings
- Solid `#141414` popups with a subtle `#333333` outline and bottom-up motion
- Configurable accent color, adaptive panel width, optional notification and weather
  modules, temperature unit, weather application, priority banners, pin visibility,
  and Control Center entries

## Compatibility

The 1.1.0 binary bundle is built for x86_64 against Plasma 6.7.4, Qt 6.11.2,
and KDE Frameworks 6.29. Temperance contains a native C++ plugin and must be
rebuilt after incompatible Qt or Plasma upgrades.

The source requires Qt 6.9 or newer and KDE Frameworks 6.20 or newer. It targets
a horizontal Plasma panel on Wayland.

## Build from source

Install a C++20 compiler, CMake, Ninja, ECM, Plasma and PlasmaQuick development
files, Qt 6 Core/DBus/Gui/Quick/Widgets development files, KDE Frameworks 6
development files, PulseAudioQt, and NotificationManager development files.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
```

Install the compiled target using a compatible native package. Native Plasma
plugins are system-installed; they are not ordinary user-local `.plasmoid`
archives.

Create the tested, installable release archive with:

```sh
./packaging/build-release.sh
```

## Configure

Right-click the widget and choose **Configure Temperance**.

- **Tray Entries** controls which native tray entries are shown, placed in the
  popup, or disabled.
- **Appearance** controls the highlight color, adaptive panel width, optional
  notification and weather modules, weather unit and application,
  important-system-alert behavior, popup pin, and which tray entries become
  Control Center pills. Control Center and System Tray remain permanently available.

When the optional online weather fallback is enabled, Temperance sends the
configured weather station coordinates or location name to Open-Meteo to obtain
the current temperature. Disable the fallback to keep weather data local to the
installed Plasma Weather widget.

Because the package identity changed in 1.1.0, remove the previous package and
add Temperance to the panel as a new widget after installing this release.

## Distribution

Compiled widgets are distributed through source control and native package
channels rather than as ordinary widget-store archives.

## Architecture and attribution

The tray host is derived from KDE Plasma Workspace 6.7.4 (tag `v6.7.4`, commit
`fd05f4c88ab093aee23ce137bf6f2412437c9bba`). Presentation code lives in `qml/`
and the inherited native tray implementation lives in `src/` and
`libdbusmenuqt/`.

See individual SPDX headers, the complete texts in [LICENSES](LICENSES), and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for licensing and required
third-party attribution.
