# Agent instructions

Applies to the whole repository.

## Startup

Read, in order:

1. `AGENTS.md`
2. `docs/ARCHITECTURE.md`
3. `docs/ROADMAP.md`, then `ROADMAP-CC.md` § Current target and Block 7 only,
   and task detail, kept privately in `../shuffle/docs/suite/`; when absent, say
   so.

Then read the task's source, and `docs/INPUT.md` for anything touched, clicked or
typed. `docs/README.md` routes every other document.

## Rules

- Product behavior and visual direction are approved before implementation
  begins.
- Preserve behavior unless the task explicitly changes it. Reproduce a defect and
  measure the property controlling it before changing it.
- One implementation owner; a second worker is read-only review or a disjoint file
  set.
- `docs/INPUT.md` defines every input and opens with the controls map; a README
  may repeat the map.
- Public text describes the product, without names, approvals or narration;
  `tests/verify-public.py` checks it. History lives in Git.
- Preserve licensing, ABI, packaging, ownership, geometry, and accessibility
  invariants unless a roadmap item changes them.

## Work packets

Keep assignments compact: repository and roadmap item; required
outcome; task-relevant contracts; acceptance checks; stop conditions; permissions
already granted. Omit history and unrelated reading.

Run `./verify.sh` before treating any change as complete. It builds and runs
CTest, so it needs the Qt and KDE development stack.
