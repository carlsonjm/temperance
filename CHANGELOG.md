# Changelog

All notable changes to Temperance are documented here.

## Unreleased

- Kept normal actionable notifications in the ticker and critical alerts in
  banners; minimized banners remain in history without another ticker pass.
- Tracked automatic ticker playback by notification identity to prevent replay
  after new arrivals or model resets; removed inner long-text clipping.
- Added history-card default actions and desktop-entry launch fallback, app
  labels for single notices, and explicit dismissal of active alerts via Clear.
- Let notification history grow with expanded groups up to the available screen
  height, preserving edge clearance and scrolling beyond that limit.
- Joined bell/arrow hover areas and reset the reading pause after paging.
- Stacked banners with 4 px internal gaps and 10 px edge clearance, adding
  card entrance/settling and history-row transitions. Guarded banner windows
  against zero-height Wayland geometry; window height itself is not animated.
- Added isolated packaged-QML loading, notification identity regression, and
  source safety checks. These supplement, not replace, live Plasma testing.

- Matched Control Center's bottom pill clearance to the System Tray.
- Reordered System Tray sections as Apps, Devices, and System, with empty
  sections removed from the spacing hierarchy.
- Separated priority-banner minimize and dismiss behavior so important alerts
  do not duplicate in the ticker and can be retained for review.

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
