# Temperance boundary with Ambient Tette

Product boundary approved September 15, 2026. The mirrored responsive geometry
was implemented in integrated commit `fb2d4aa` (accepted candidate `a56288e`)
and physically accepted by J on September 15, 2026.

Temperance answers **what changed**. Today it implements notification intake,
the left-side notification ticker, priority banners, and notification history.
It does not yet provide a general system-event or state-transition feed.

Ambient Tette answers **what matters now**. It owns ongoing relevance and current
context on the right side of the dock: media playback, file transfers, and other
live activity. Temperance must not become the activity model or duplicate live
progress.

Target examples after Temperance gains a separate event slice:

- A transfer begins: Temperance may announce the transition; Ambient Tette keeps
  authoritative progress and Cancel visible.
- A transfer completes, fails, or is canceled: Ambient removes the live item;
  Temperance or the source's notification route communicates the outcome.
- Media starts: Ambient keeps the session and actions visible. Temperance does
  not continuously repeat media state.
- Battery threshold, network change, or device event: Temperance communicates
  the transition. Ambient does not retain static system status.

Source applications and services own underlying state and actions. Temperance
must not infer live progress from notification text, run a second job owner, or
duplicate an outcome already presented through the shared notification model.

The center application dock remains physically centered. Temperance measures and
flexes inside its left-side allocation using event-driven panel geometry. On the
right, the accepted order is `Tette launcher → responsive Ambient strip → task
dock`. Ambient activity expansion uses one compact popup per activity; the
combined full-screen activity view was removed before acceptance.

The accepted compositor remains fixture-only through
`TETTE_AMBIENT_FIXTURE=transfer-media`. Real activity providers and source action
routing remain A2 work in Tette and are not part of this freeze.

## Future event-source slice

Before implementation, inventory supported authoritative sources for system
transitions and activity outcomes. Define event identity, freshness, priority,
deduplication against existing notifications, dismissal, and accessibility.
Implement a small source set first. Do not turn periodic status polling into fake
events or make this slice a dependency of the initial Ambient Tette release.

Ambient implementation belongs in Tette. Temperance changes are limited to a
later proven event handoff or duplicate-suppression need, reviewed as an
independent packet.
