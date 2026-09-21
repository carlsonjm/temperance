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

Claude Code loads `CLAUDE.md` automatically. It routes into this same startup set
and adds no separate protocol.

## Rules

- Preserve behavior unless the task explicitly changes it.
- Keep one implementation owner. Add `SWARM.md` entries only when another live
  agent must act; remove completed handoffs.
- Keep this repository's plan in `NEXT-ROADMAP.md`, current truth in
  `CURRENT_STATE.md`, and durable rationale in `DECISIONS.md`. Suite block order
  and cross-repository dependencies are owned by `ROADMAP-CC.md` in the Kadunce
  repository; read it when that checkout is present.
- Leave history to Git and `docs/archive/`; keep narration out of live docs/code.
- Preserve licensing, ABI, packaging, ownership, geometry, and accessibility
  invariants unless a roadmap item changes them.

## Work packets

Keep assignments compact and ordered: repository and roadmap item; required
outcome; task-relevant contracts; acceptance checks; stop conditions; permissions
already granted. Omit history and unrelated reading. If another worker must act,
reduce the dependency to one `SWARM.md` handoff and delete it when resolved.

Run `./verify.sh` before treating a source, packaging, test, or documentation
change as complete.
