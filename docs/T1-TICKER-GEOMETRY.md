# T1: accepted ticker geometry correction

Base: `10fb70f0e5cc602afd68991b6c0ea154a88c73dc`.
Accepted candidate: `8022fc649e375e489a22a498c930dff1111f8845`.
J physically passed September 16: ticker is back to normal with both stock
spacers expanding. Freeze publication adds documentation only.

## Correction

`availablePanelWidth()` measures scene coordinates, but the event watcher only
observed applet roots. Plasma can move their AppletContainer ancestors without
changing the roots' local x or width. The cached adaptive width then stays stale.
`watchGeometryItem()` now observes the ancestor chain and parent changes; the
existing queued refresh discovers newly assigned ancestors. Connections remain
deduplicated and guarded by QPointer. No polling or new layout policy is added.

The measured boundary, control floors, clipping, scrolling, compact mode and
popup contract are unchanged. Production QML adds only test object names.
Tette, spacers and panel configuration are unchanged.

## Validation

- Full RelWithDebInfo build passed.
- All five existing CTest tests passed (private buses required outside sandbox).
- Regression on base plus instrumentation: moving the real task AppletContainer
  40 logical pixels, with zero child x/width signals, produced zero Temperance
  invalidations and failed. Corrected source emits one invalidation and passes.
- Real org.kde.panel, expanding stock spacers, task manager, clock, Temperance,
  and unchanged accepted Tette plugin `7f54caa` passed at 1280x800 DPR 1,
  1920x1080 DPR 1 on second output, and 1920x1080 DPR 1.5 on second output
  (1280x720 logical). Each run passed all three QtTest cases.
- Each run exercises adaptive on/off, task demand 108/216/432/216, collapsed and
  expanded arrows, rendered ticker ink, clipped content, no task overlap, and
  a real private-bus notification whose long text scrolls.
- Maximum adaptive measurement error: 1 logical pixel. Actual root allocation
  is 4–6 pixels below the requested width due to stock layout gaps; content
  follows the actual allocation. This difference is not the reported collapse.
- Package staged only under `/tmp/temperance-t1-package`: expected plugin and
  icon, plugin byte-identical to build output. Shell syntax and diff checks pass.

Run the fixture from this worktree:

```sh
bash tests/run-ticker-panel-test.sh /tmp/temperance-t1-build/bin/temperance-ticker-panel-test /tmp/tette-t1-baseline-build/bin /tmp/temperance-t1-build/bin 1280 800 1 0
```

Other matrix suffixes: `1920 1080 1 1` and `1920 1080 1.5 1`.
Evidence excerpts are in `T1-TICKER-EVIDENCE.txt`.

## Physical acceptance and scope

The test reproduces the missing parent-only geometry invalidation, not the exact
live startup sequence. J's installed September 16 test supplies physical evidence:
the ticker is back to normal with expanding spacers. The installed plugin was
compared byte for byte with `/tmp/temperance-t1-build` and matched. Documentation
freeze does not change production sources. No further live install, restart,
configuration change or logout is performed during publication.

Acceptance closes the ticker regression only. Notification layout, popup
clearance, casing, power-icon scale, spacing and other polish remain open.

Rollback: build accepted commit `10fb70f` in a separate checkout/build directory,
install that Temperance artifact and restart Plasma. No branch reset or spacer
configuration change is needed.
