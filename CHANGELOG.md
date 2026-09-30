# Changelog

This file records user-visible changes.

## Unreleased

- The battery opens Plasma's Power and Battery page, as Control Center's battery
  row does; the network icon opens Control Center. A wired connection shows
  Lucide's ethernet port, drawn with the bell's line. The notification count
  sits a little lower under the bell, and Control Center's power pill is a
  flat grey pill like the title bar's.
- A sideways flick, by finger or mouse, checks the ticker's line off, unread in
  the history, and brings the next up; the arrow is gone, and its width is the
  ticker's. Once every line is checked off the bell shows ✓ until the history
  is opened. Opening the history clears the ticker, and a finger on the bell no
  longer brings its line back.
- A transfer's end, and a drive plugged in that nothing opened, are filed in
  the history without crossing the ticker: Ambient has already shown each for
  a minute.
- Corners follow the suite's three tiers: Control Center and alerts at 12 px,
  the cards inside them at 8 px, and every control a pill.
- Sized the status icons for touch: a third larger, set as one group, each
  answering a touch across the panel's height, and lifting under the finger.
- Drew the network icon as a bold symbolic mark, the bell's count larger, and
  the battery in the clock's weight.
- Led the ticker's event with Now, the minutes to its start, or its start in
  the clock's form, and a notification with its application, ending a line too
  long for the ticker in an ellipsis.
- Added Temperance's own clock at the Status Bar's right end, its new minute dealt
  like a card, and the same clock as a block for Shuffle Lock.
- Added a month calendar card with holidays and read-only events from linked
  private calendar addresses, each with a chosen color.
- Brought today's next timed event into the ticker and today's events into the
  history, now titled Notifications & events.
- Put every popup page on the calendar's margins and spacing.
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
