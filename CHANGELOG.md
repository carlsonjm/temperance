# Changelog

This file records user-visible changes. Validation logs, candidate investigations,
and physical acceptance records are indexed in `docs/archive/`.

## Unreleased

- Added producer-named notification actions and individual dismissal to standalone
  and grouped history cards while preserving default-action and application
  fallback behavior.
- Kept wrapped notification action controls inside their cards with 44 px input
  targets, visible focus, and distinct hover/pressed states.
- Improved notification card rhythm, bounded priority-banner content, popup
  clearance, session glyph sizing, and suite casing.
- Made adaptive width refresh when Plasma moves an ancestor AppletContainer
  without changing the applet root's local geometry.
- Added native Bluetooth pairing access through the installed KDE wizard.
- Made formatted notification text expandable and improved bell-count geometry.
- Added a pinned Lucide subset for suite-owned action chrome while protecting the
  animated bell, weather treatment, and custom tray work.
- Mirrored the panel allocation contract used by Ambient Tette without adding
  cross-repository runtime ownership.

## 1.1.0 — 2026-09-08

- Rebranded the widget and package as Temperance.
- Added neighbor-aware adaptive width and retained configurable fixed-width mode.
- Made Control Center order follow the configured pill order.
- Combined weather/temperature and bell/count into compact live controls.
- Added independent notification and weather visibility settings.
- Made Control Center height follow configured rows.
- Added capability-gated battery and performance controls.
- Added configurable session controls and native tray visibility settings.
- Added priority banners, matching popup surfaces, reproducible native packaging,
  and the Temperance widget artwork.
- Preserved upstream platform identifiers, licensing, and third-party attribution.

## 1.0.0 — 2026-09-08

- Introduced the adaptive status rail, notification ticker and history, weather,
  network, and battery status.
- Added Control Center controls for volume, brightness, connectivity, updates,
  battery, and supported performance profiles.
- Added the categorized System Tray, configurable Control Center pills, Do Not
  Disturb, popup pinning, accent color, and notification/weather settings.
- Added native x86_64 release packaging.
