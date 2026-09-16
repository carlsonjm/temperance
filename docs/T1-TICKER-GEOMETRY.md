# T1: ticker geometry invalidation candidate

Base: `10fb70f0e5cc602afd68991b6c0ea154a88c73dc`.
Branch: `a/temperance-t1`.

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

## Limit and physical gate

The test reproduces the missing parent-only geometry invalidation. It does not
reproduce J's exact 128px live-root startup sequence, nor prove this is the only
cause of that physical collapse. This is a source candidate for physical review,
not a physically accepted fix. No live install, restart, configuration change,
or push was performed.

After PM/J installs, with restored expanding spacers: verify the ticker fills
its assigned free span; expand/collapse arrows; send long text and verify scroll;
grow/shrink tasks and confirm no overlap; repeat adaptive off/on and tablet /
monitor / fractional scale. Reject if the tiny strip persists after restart.

Rollback: build accepted commit `10fb70f` in a separate checkout/build directory,
install that Temperance artifact and restart Plasma. No branch reset or spacer
configuration change is needed.
