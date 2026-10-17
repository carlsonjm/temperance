# Temperance

Native Plasma widget build for x86_64 systems using Plasma 6.7.4, Qt 6.11.2,
and Kirigami 6.29.

## Install

Temperance needs KDE's calendar library, packaged as `kcalendarcore`. The
installer stops and says so if it is missing.

Double-click `install-system.sh` and choose **Execute**, or open this folder in a
terminal and run `./install-system.sh`. A terminal stays open so you can see the
authorization prompt, the result or any error. The installer puts Temperance
where the system's Qt looks for plugins and restarts the panel. Your windows and
session carry on, so an update needs no sign-out. It reports success only when the installed
files match this package, and says whether the restarted panel has loaded them.

The first time, open panel edit mode, choose **Add Widgets**, search for
**Temperance**, and drop it into the panel.

**Configure Temperance**, in the widget's menu, selects the highlight color,
promotes tray entries into Control Center pills, and links calendars. Promoted
entries leave the organized tray.

The package identity changed in 1.1.0. To upgrade from 1.0.0, remove the old
package and add Temperance to the panel as a new widget.

## Remove

Run `./uninstall-system.sh`. It puts Plasma's System Tray back on Temperance's
panel, so notifications show again, and restarts the panel.

This plugin runs only on the architecture, Plasma and Qt it was built for; the
installer warns if yours differ.

The included PNG is Temperance's icon in Add Widgets.
