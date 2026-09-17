# Historical evidence index

These files preserve dated freezes, physical checks, raw test output, and
superseded implementation investigations. They may contain old branch names,
commit IDs, temporary paths, ownership assignments, or next steps. Treat those as
provenance, not current instructions.

Use the canonical documents first. Consult this directory only to diagnose a
regression, answer a provenance question, or compare a failed candidate.

## Path mapping

| Former path | Archived path | Durable contract extracted to |
|---|---|---|
| `docs/FROZEN-2026-09-10.md` | `docs/archive/FROZEN-2026-09-10.md` | `../DECISIONS.md` — Control Center, tray, session behavior |
| `docs/QOL-20260913.md` | `docs/archive/QOL-20260913.md` | `../DECISIONS.md` — notification history and native pairing |
| `docs/NOTIFICATION-HISTORY-ACTIONS-20260915.md` | `docs/archive/NOTIFICATION-HISTORY-ACTIONS-20260915.md` | `../DECISIONS.md` — producer actions and popup placement |
| `docs/NOTIFICATION-PRESENTER-HANDOFF.md` | `docs/archive/NOTIFICATION-PRESENTER-HANDOFF.md` | `../DECISIONS.md` and `../NEXT-ROADMAP.md` — single-presenter lifecycle |
| `docs/T1-TICKER-GEOMETRY.md` | `docs/archive/T1-TICKER-GEOMETRY.md` | `../DECISIONS.md` — scene-space adaptive width |
| `docs/T1-TICKER-EVIDENCE.txt` | `docs/archive/T1-TICKER-EVIDENCE.txt` | Raw evidence referenced by the archived geometry record |

The files were moved without rewriting their evidence. Git rename history and the
table above preserve old references. No historical Git commit was changed.
