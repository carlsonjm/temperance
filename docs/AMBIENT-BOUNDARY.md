# Temperance boundary with Ambient Tette

This document defines the live ownership boundary between Temperance and Ambient
Tette.

Temperance answers **what changed**. It owns notification intake and presentation:
the left-side ticker, priority banners, grouped history, dismissal, and producer
actions. A future bounded source may report authoritative system transitions, but
Temperance must not turn continuous status into synthetic events.

Ambient Tette answers **what matters now**. It owns ongoing context on the right
side of the dock, including media sessions, transfers, their progress, and their
live actions.

Preserve these invariants:

- Source applications and services own underlying state and actions.
- Temperance must not infer progress from notification text, become a second job
  owner, or continuously repeat live media state.
- Ambient removes an activity when its authoritative source ends. It must not
  invent completion from filesystem quiet time or source loss.
- A transition outcome may enter Temperance through the shared notification path,
  but the same outcome must not be presented twice.
- Temperance changes for Ambient integration are limited to a proven event handoff,
  duplicate suppression, or shared geometry contract. Ambient implementation stays
  in Tette.

## Panel geometry

The center application dock remains physically centered. Temperance measures and
flexes only inside its left-side allocation. On the right, Tette owns its launcher
and Ambient strip before the task dock. Each side reacts to panel geometry without
moving, sizing, or configuring the other side.

Temperance must preserve event-driven geometry updates and must not poll the panel,
write spacer configuration, or claim the center allocation. Surplus width stays
outside complete activity/control groups rather than being inserted between their
children.

## Event-source gate

Before adding a non-notification source, define its authoritative provider, event
identity, freshness, priority, deduplication, dismissal, and accessibility. Start
with a small source set and keep the work independent of Ambient's initial provider
support. Execution belongs in `NEXT-ROADMAP.md`.
