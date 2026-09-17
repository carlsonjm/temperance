# Agent instructions

These instructions apply to the entire Temperance repository.

## Startup order

1. Read `AGENTS.md`.
2. Read the canonical current roadmap/state document: `docs/CURRENT_STATE.md`.
3. Read `SWARM.md`.
4. Read only source/docs relevant to the assigned task.
5. Do not reconstruct historical context unless one of those sources explicitly requires it.

## Coordination

- Use `SWARM.md` only for currently active cross-agent handoffs and runtime coordination.
- Keep no more than three live handoffs, with no more than 50 words per handoff.
- Delete completed handoffs instead of archiving them in `SWARM.md`.
- Record durable architectural and engineering decisions in the canonical decision or architecture documents.
- Let Git history record implementation changes.
- Keep code comments limited to code behavior and reasoning. Never put agent conversation, handoffs, PM instructions, implementation history, or authorship notes in code comments.

## Scope discipline

Preserve existing behavior unless the assigned task explicitly requires a behavior change. Keep edits focused on the assigned source and documentation.
