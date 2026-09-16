# Current state — September 16, 2026

J physically accepted T1 **8022fc6**: the ticker is back to normal with both
stock spacers expanding. Base is **10fb70f**. The installed plugin matches the
accepted source build byte for byte. This documentation freeze changes no
production source and requires no further installation.

`SystemTray::watchGeometryItem()` now observes the ancestors that Plasma moves,
so scene-space adaptive width is invalidated even when root-local geometry is
unchanged. Existing width policy, controls, clipping, scrolling and popup behavior
remain intact. No Tette, shared-center or spacer-tooling changes are included.

Build, five existing CTests, real-panel tablet/monitor/DPR1.5 checks, source/diff
and staged-package checks passed for the candidate; publication repeats the
final-head build, full CTest and package checks. The real-panel test covers task
growth/shrink, adaptive on/off, arrow states and long real notification scrolling.
See `T1-TICKER-GEOMETRY.md` and `T1-TICKER-EVIDENCE.txt`.

Rollback: build/install **10fb70f** from a separate checkout and use the normal
Plasma restart procedure. No branch reset or spacer configuration change.

Next bounded work remains PM-assigned notification/visual polish: distinct recent
cards, popup fit/clearance, lowercase labels, power glyph scale, and consistent
Control Center/tray spacing. Ticker acceptance does not close those items.
