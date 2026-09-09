# Changelog

All notable changes to Temperance are documented here.

## 1.1.0 — 2026-09-08

- Rebranded the widget and package as Temperance.
- Replaced product-specific identifiers, copy, defaults, and demo content.
- Made Control Center ordering follow the user's configured pill order.
- Separated neighbor-aware responsive sizing from the manual fixed-width mode.
- Made responsive mode follow the live task-dock boundary without a target width.
- Standardized compact status controls on equal-width, background-free icon slots.
- Let the panel layout continuously resize the ticker around neighboring widgets.
- Moved notification controls to the status edge so they no longer anchor in open space.
- Combined weather condition and temperature into one overlapping live icon.
- Combined the notification bell and counter into one always-visible live icon.
- Restored the full bell at rest and animated its clapper into the live counter state.
- Added independent status-area toggles for Notifications and Weather.
- Made Control Center popup height follow the number of configured tile rows.
- Replaced the compact battery illustration with a centered percentage readout.
- Gated the performance-profile tile on successful local helper discovery.
- Hid the Control Center battery tile when no system battery is present.
- Renamed device-specific internal interfaces to generic performance controls.
- Preserved required platform identifiers and upstream license attribution.
- Restored important-alert banners and enforced their configured lifetime.
- Added a matching border to important-alert banners.
- Added source sanity checks and reproducible release packaging.
- Added the custom Temperance artwork as the widget-browser icon.

## 1.0.0 — 2026-09-08

- Introduced the 400 px adaptive status rail with weather, notification paging,
  network state, and compact battery gauge.
- Added animated notification intake, delayed overflow reading, read-state
  paging, and app-grouped notification history with per-app clearing.
- Added Control Center volume, brightness, connectivity, update, battery, and
  performance-profile controls.
- Added the categorized System Tray and native tray-entry configuration.
- Added matching solid popups, bottom-up motion, DND control, optional pinning,
  configurable accent color, and configurable Control Center pills.
- Added settings for panel width, temperature units, preferred weather app,
  priority banners, and banner lifetime.
- Added a reusable bell glyph and native x86_64 release packaging.
