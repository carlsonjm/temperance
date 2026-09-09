# Temperance 1.1.0

Native Plasma widget build for x86_64 systems using Plasma 6.7.4, Qt 6.11.2,
and Kirigami 6.29.

## Install

Double-click `install-system.sh` and choose **Execute**, or open this folder in a
terminal and run `./install-system.sh`. A terminal remains open so authorization,
success, or any error is visible. Then sign out and back in, open panel edit mode,
choose **Add Widgets**, search for **Temperance**, and drop it into the panel.

Right-click the widget and choose **Configure Temperance** to select the highlight
color and promote tray entries into Control Center pills. Promoted entries are
automatically removed from the organized tray.

The package identity changed in this release. Remove the previous package and
add Temperance as a new widget rather than updating the old panel instance.

## Remove

Run `./uninstall-system.sh`, then restart Plasma or sign out and back in.

This is a native, architecture-specific plugin. Rebuild it after major Qt or
Plasma ABI upgrades.

The included PNG is installed as Temperance's widget-browser icon. Compact
panel controls continue using their purpose-built monochrome system icons.
