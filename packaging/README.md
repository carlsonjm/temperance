# Temperance

Native Plasma widget build for x86_64 systems using Plasma 6.7.4, Qt 6.11.2,
and Kirigami 6.29.

## Install

Temperance needs KDE's calendar library, packaged as `kcalendarcore`. The
installer stops and says so if it is missing.

Double-click `install-system.sh` and choose **Execute**, or open this folder in a
terminal and run `./install-system.sh`. A terminal remains open so authorization,
success, or any error is visible. The installer then restarts the panel to load
the new build; windows and the session are untouched, so an update needs no sign
out. It reports success only once the system holds this package's own files, and
says whether the restarted panel has loaded them. The first time, open panel edit mode, choose **Add Widgets**, search for
**Temperance**, and drop it into the panel.

**Configure Temperance**, in the widget's menu, selects the highlight color,
promotes tray entries into Control Center pills, and links calendars. Promoted
entries are automatically removed from the organized tray.

The package identity changed in 1.1.0. When upgrading from 1.0.0, remove the
previous package and add Temperance as a new widget rather than updating the old
panel instance.

## Remove

Run `./uninstall-system.sh`, then restart Plasma or sign out and back in.

This is a native, architecture-specific plugin. Rebuild it after major Qt or
Plasma ABI upgrades.

The included PNG is installed as Temperance's widget-browser icon. Compact
panel controls continue using their purpose-built monochrome system icons.
