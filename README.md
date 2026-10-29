<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/readme/banner-dark.png">
    <img src="assets/readme/banner-light.png" alt="Temperance: a quiet Status Bar for KDE Plasma" width="100%">
  </picture>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/KDE_Plasma-6-1d99f3?style=flat" alt="KDE Plasma 6">
  <img src="https://img.shields.io/badge/input-touch_first-00b894?style=flat" alt="Touch first">
  <img src="https://img.shields.io/badge/license-GPL--2.0--or--later-blue?style=flat" alt="GPL-2.0-or-later">
</p>

<p align="center">
  <a href="#install">Install</a> · <a href="#controls">Controls</a> · <a href="docs/README.md">Docs</a> · <a href="#used-in-shuffle">Used in Shuffle</a>
</p>

<p align="center">Temperance is a Status Bar for KDE Plasma 6: notifications, weather, battery, common controls, the System Tray and the time, in one panel widget.</p>



https://github.com/user-attachments/assets/940503f2-f2d6-489e-9c71-e531ad197180



## What you get

<table>
<tr>
<td width="33%" valign="top"><b>Notifications &amp; Events</b><br>Notifications pass through the bar, then wait in a history under today's events.</td>
<td width="33%" valign="top"><b>Control Center</b><br>Volume, brightness, connections and, where supported, performance profiles. Sleep, Restart and Shut down sit in its header.</td>
<td width="33%" valign="top"><b>System Tray</b><br>App and device icons, each shown in the bar, the popup, or nowhere.</td>
</tr>
<tr>
<td width="33%" valign="top"><b>Clock and calendar</b><br>The time, and the date if you like. Tap for a month with holidays and your calendars.</td>
<td width="33%" valign="top"><b>Weather</b><br>Current conditions and temperature, off until you turn it on.</td>
<td width="33%" valign="top"><b>Fits the panel</b><br>Fills the free space, slims for thin panels and stops pages above a bottom dock.</td>
</tr>
</table>

It replaces Plasma's System Tray presentation and brings its own clock, which you can turn off.

## Controls

| Task | Touch | Keyboard |
| --- | --- | --- |
| Notifications & Events | Tap the bell | — |
| Control Center | Tap the network icon | — |
| System Tray | Tap the tray icon | — |
| Calendar | Tap the clock | — |
| Weather | Tap the weather icon | — |

Tap an open page's icon again to close it. Every other input is in [docs/INPUT.md](docs/INPUT.md).

## Install

These steps are for Arch Linux or CachyOS running KDE Plasma 6.7 or newer in a
Wayland session, the default on both. Temperance is built on your computer
from this page's code, so you type a few commands into **Konsole**, Plasma's
terminal. Open it from the application launcher.

### 1. What you need

Check your Plasma version. It must say 6.7 or higher:

```bash
plasmashell --version
```

Bring the computer up to date. If you just did this for another part of the
suite, skip it:

```bash
sudo pacman -Syu
```

Restart the computer only if that update installed a new `kwin`,
`plasma-workspace` or kernel (`linux`, or `linux-cachyos` on CachyOS): Temperance is built against what is
installed, so that has to be what is running.

Then install what the build needs. Type your password when asked, and answer
`Y`:

```bash
sudo pacman -S --needed base-devel git cmake ninja python dbus extra-cmake-modules qt6-base qt6-declarative qt6-tools kcalendarcore kconfig kcoreaddons kiconthemes ki18n kitemmodels kjobwidgets kio kservice kstatusnotifieritem kwindowsystem kxmlgui kpackage libplasma plasma-workspace pulseaudio-qt libkscreen layer-shell-qt powerdevil kdeclarative kcmutils kirigami ksvg breeze-icons kdeplasma-addons ttf-dejavu noto-fonts
```

If pacman says a package **is up to date -- skipping**, or **there is nothing
to do**, that is normal: it is already installed.

### 2. Download

This puts the code in a folder called `temperance` in your home folder. Use
`git`, not GitHub's Download ZIP: the build reads the download's history.

```bash
cd ~
git clone https://github.com/carlsonjm/temperance.git
cd temperance
```

### 3. Install

Build it, which takes a few minutes and ends with **Built
release/Temperance-…tar.gz**:

```bash
./packaging/build-release.sh
```

Then install what it built. It asks for your password in Konsole (nothing
shows as you type) and restarts the panel, which blinks out for a moment; your
windows stay open:

```bash
cd release/Temperance-*/
./install-system.sh
```

It ends with **Installed Temperance and restarted the panel**.

### 4. Put it on the panel

1. Right-click an empty part of the panel and choose **Add Widgets…**.
2. Search for **Temperance** and drag it onto the panel. It needs a
   horizontal panel, such as Plasma's own along the bottom.
3. Temperance brings its own tray, notifications and clock. Remove Plasma's
   **System Tray** and **Digital Clock** from that panel, so they don't show
   twice: right-click each and choose **Remove**, or remove them in the
   panel's edit mode.

### 5. Check it worked

- The bar shows a bell, a network icon, the battery where there is one, a tray
  icon and the time.
- Tap the clock: a month calendar opens. Tap it again to close it.
- Tap the network icon: Control Center opens, with volume, brightness and
  connections.

**Configure Temperance**, in the widget's right-click menu, sets the highlight
colour, which tray entries show, and linked calendars.

### 6. Update

After every Plasma or Qt update, the panel says Temperance needs an update.
Build and install it again, which also brings in a newer Temperance:

```bash
cd ~/temperance
git pull
./packaging/build-release.sh
cd release/Temperance-*/
./install-system.sh
```

Upgrading from 1.0.0: remove the old widget from the panel first, then add
Temperance as a new widget.

### 7. Uninstall

```bash
cd ~/temperance
./packaging/uninstall-system.sh
```

It asks for your password, takes Temperance off every panel and puts Plasma's
System Tray back in its place, so notifications show again. Put the clock back
from **Add Widgets…**: **Digital Clock**. To delete the download too, run
`rm -rf ~/temperance`.

## Details

<details>
<summary>Status Bar and notifications</summary>

The bar shows recent notifications, weather, battery, tray entries, and the time over the optional date, in the system font or one you choose. If enabled, volume or brightness changes show briefly. Turned-off sections give their space to the rest.

An app's notifications group together and keep the app's own actions, such as reply. Critical and selected system alerts show as larger banners, timed or kept until dismissed. The bell shows how many are waiting, or just the bell.
</details>

<details>
<summary>Calendar, Control Center and tray</summary>

The calendar is a small card in the corner. Link calendars by pasting their private iCal address from Google, iCloud or Outlook; they are read-only and refresh every ten minutes. Today's next timed event shows in the bar.

Control Center's header always shows Shut down and keeps Lock and Log out behind More. Each other control can stand, wait behind More, or hide. Controls the machine lacks are left out. Control Center and the tray work with notifications or weather off.
</details>

<details>
<summary>Settings and privacy</summary>

**Configure Temperance**, in the widget's menu, opens Shuffle Settings where installed, otherwise:

- **Tray Entries**: where each tray entry appears.
- **Appearance**: session controls, highlight color, width, notification and weather sections, the bell, temperature unit, weather app, clock and font, important alerts, popup pin, Control Center entries.
- **Calendar**: linked calendars and their colors.

With the online weather fallback on, Temperance sends the configured station coordinates or location name to Open-Meteo hourly when Plasma's Weather widget has no temperature. Turn it off to keep weather local.
</details>

## For developers

<details>
<summary>Building, checks and attribution</summary>

Read `AGENTS.md` first; `docs/README.md` indexes the docs.

Building needs C++20, CMake, Ninja, ECM, Qt 6.9 and KDE Frameworks 6.20 or newer, with development files for Plasma, PlasmaQuick, Qt Concurrent/Core/DBus/Gui/Network/Quick/Widgets, KCalendarCore, PulseAudioQt, libkscreen, and Plasma Workspace's NotificationManager and KWorkspace; the tests add LayerShellQt.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
./verify.sh                    # the repository checks
./packaging/build-release.sh   # the release archive
```

The tray host in `src/`, `libdbusmenuqt/` and `qml/` derives from KDE Plasma Workspace 6.7.4; `tools/upstream.sh` records it file by file.
</details>

## Used in Shuffle

Temperance works on any Plasma 6 panel. [Shuffle](https://shuffleplasma.com), a touch-first desktop built on KDE Plasma, uses it as its Status Bar, alongside the open-source [Kadunce](https://github.com/carlsonjm/kadunce), [Tettegouche](https://github.com/carlsonjm/tettegouche), [Gooseberry](https://github.com/carlsonjm/gooseberry), [Shuffle Keyboard](https://github.com/carlsonjm/shuffle-keyboard) and [Split Rock](https://github.com/carlsonjm/split-rock).

## License

Licensed GPL-2.0-or-later ([LICENSE](LICENSE)); see the SPDX headers, [LICENSES](LICENSES), and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for per-file terms and attribution. The project names and marks are not covered by that licence; see [TRADEMARKS.md](TRADEMARKS.md).

<p align="center"><picture><source media="(prefers-color-scheme: dark)" srcset="assets/readme/good-input-white.png"><img src="assets/readme/good-input-ink.png" alt="Good Input" width="140"></picture></p>
