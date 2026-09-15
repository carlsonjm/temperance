# Temperance boundary with Ambient Tette

Product boundary approved September 15, 2026.

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

The center application dock remains physically centered. Temperance may flex only
inside its left-side allocation while Ambient Tette flexes inside the right-side
allocation.

## Future event-source slice

Before implementation, inventory supported authoritative sources for system
transitions and activity outcomes. Define event identity, freshness, priority,
deduplication against existing notifications, dismissal, and accessibility.
Implement a small source set first. Do not turn periodic status polling into fake
events or make this slice a dependency of the initial Ambient Tette release.

Ambient implementation belongs in Tette. Temperance changes are limited to a
later proven event handoff or duplicate-suppression need, reviewed as an
independent packet.
