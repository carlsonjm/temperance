# Temperance — Claude Code entry point

`AGENTS.md` is the authoritative startup and working contract for this repository
and applies unchanged to Claude Code. Read it first. This file adds only the suite
context and session facts `AGENTS.md` does not carry.

## Which checkout

The working tree is `Projects/Shuffle/temperance`. A second checkout at
`Projects/Itasca/temperance` predates the current execution plan. Confirm the path
before reading or editing.

## Suite position

Temperance is one of three open-source component repositories in the Shuffle
suite. Block order across the suite is owned by
`../kadunce/docs/ROADMAP-CC.md`. The Temperance event boundary is its Block 7,
which is ready and is the smallest remaining component block.

`docs/NEXT-ROADMAP.md` holds this repository's execution plan and task detail.
`ROADMAP-CC.md` decides which block is next; `NEXT-ROADMAP.md` decides what the
work inside it is. When only this repository is checked out, work from
`NEXT-ROADMAP.md` and say that the suite plan was unavailable.

Tettegouche owns live progress and actions. Neither side may retain the same
completion indefinitely; `docs/AMBIENT-BOUNDARY.md` is the live contract for that
split.

## Verification

Run `./verify.sh` before treating a source, test or documentation change as
complete. It configures, builds and runs CTest, so it needs a local session with
the Qt and KDE development stack.

`tests/verify-docs.py` enforces index coverage: a new tracked document must be
added to `docs/README.md` in the same change.

## Suite rules

- J approves product behavior and visual direction before implementation begins.
- One implementation owner per repository; a second worker is read-only review or
  a disjoint file set.
- Reproduce a defect and measure the property controlling it before changing it.
- Preserve licensing, ABI, packaging, ownership, geometry and accessibility
  invariants unless a roadmap item changes them.
- A package identity change forces users to remove and re-add the widget, as
  1.1.0 showed. Identity work is Block 10b, coordinated across all three
  repositories.
