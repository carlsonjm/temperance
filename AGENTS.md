# Agent instructions

Applies to the whole repository.

## Startup

Read, in order:

1. `AGENTS.md`
2. `docs/ARCHITECTURE.md`
3. The plan and the task's detail, kept privately with the suite; when absent,
   say so.

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

## Git history

Work freely on task branches and commit as needed during implementation. Before
opening or merging a pull request, clean the branch so its history reflects
meaningful product or engineering changes, not the debugging process.

- Keep commits that represent distinct behavior, architecture, migration or
  design decisions. Squash failed attempts, test-only repair churn, typo fixes,
  CI retries, forgotten-file follow-ups and other mechanical corrections into
  the commit they belong to.
- Prefer one to three meaningful commits per pull request. Do not combine
  unrelated changes merely to reduce the count.
- Clean the branch before its head is handed over for an install or a hands-on
  check, so the head tested there is the head that merges. Rewriting a head
  after hand-over breaks the hand-over.
- Never rewrite history already merged to `main` or `next`.

## Work packets

Keep assignments compact: repository and roadmap item; required
outcome; task-relevant contracts; acceptance checks; stop conditions; permissions
already granted. Omit history and unrelated reading.

Run `./verify.sh` before treating any change as complete. It builds and runs
CTest, so it needs the Qt and KDE development stack.
