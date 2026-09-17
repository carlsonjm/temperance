# Agent instructions

Applies to the whole repository.

## Startup

Read only:

1. `AGENTS.md`
2. `docs/CURRENT_STATE.md`
3. `docs/NEXT-ROADMAP.md`
4. `SWARM.md`

Then read task-relevant source and decisions. Load visual-language guidance for
presentation work and `docs/archive/` only for regression or provenance.

## Rules

- Preserve behavior unless the task explicitly changes it.
- Keep one implementation owner. Add `SWARM.md` entries only when another live
  agent must act; remove completed handoffs.
- Keep plans in `NEXT-ROADMAP.md`, current truth in `CURRENT_STATE.md`, and
  durable rationale in `DECISIONS.md`.
- Leave history to Git and `docs/archive/`; keep narration out of live docs/code.
- Preserve licensing, ABI, packaging, ownership, geometry, and accessibility
  invariants unless a roadmap item changes them.
