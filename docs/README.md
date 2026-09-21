# Documentation index

Canonical documentation says what is true, why it is true, and which invariants
must survive a change. Git and `archive/` preserve how the repository reached
that state.

## Reading policy

Ordinary startup is limited to `AGENTS.md`, `CURRENT_STATE.md`,
`NEXT-ROADMAP.md`, and `SWARM.md`. Read `DECISIONS.md` only for the subsystem
being changed. Load visual, boundary, packaging, or legal references when the task
touches them. Archive material is opt-in for regression diagnosis, provenance, or
a failed-candidate comparison.

Run `../verify.sh` for the repository build, tests, and documentation checks. The
documentation guard enforces archive isolation, compact runtime handoffs,
current-state hygiene, and index coverage.

Update documents by role:

- **Canonical** — current truth, execution plan, durable decisions, or operating
  policy.
- **Reference** — a live product, visual, packaging, boundary, or legal contract
  needed for relevant work.
- **Historical Evidence** — releases, freezes, test output, investigations, and
  acceptance records. It may describe superseded code and is not instruction.
- **Obsolete/Redundant** — content with no live contract or unique evidence. No
  such file remains tracked after this pass.

## Inventory

| Classification | Document | Purpose |
|---|---|---|
| Canonical | `AGENTS.md` | Startup and repository working rules |
| Canonical | `CLAUDE.md` | Claude Code entry point; routes into the `AGENTS.md` startup set |
| Canonical | `SWARM.md` | Current cross-agent runtime handoffs only |
| Canonical | `docs/CURRENT_STATE.md` | Current product and limitations |
| Canonical | `docs/NEXT-ROADMAP.md` | This repository's execution plan and task detail; suite block order lives in Kadunce's `ROADMAP-CC.md` |
| Canonical | `docs/DECISIONS.md` | Durable subsystem rationale and invariants |
| Canonical | `docs/README.md` | Documentation policy and classification |
| Reference | `README.md` | Product, compatibility, build, and configuration |
| Reference | `docs/AMBIENT-BOUNDARY.md` | Live Temperance/Tette ownership boundary |
| Reference | `docs/ITASCA-VISUAL-LANGUAGE.md` | Shared presentation contract |
| Reference | `packaging/README.md` | Release artifact installation contract |
| Reference | `THIRD_PARTY_NOTICES.md` | Runtime service and derived-work notices |
| Reference | `assets/icons/THIRD_PARTY.md` | Icon-source and rendering contract |
| Reference | `LICENSES/GPL-2.0-or-later.txt` | Distributed GPL text |
| Reference | `LICENSES/ISC.txt` | Distributed ISC text |
| Reference | `LICENSES/LGPL-2.0-or-later.txt` | Distributed LGPL text |
| Reference | `assets/icons/lucide/LICENSE` | Vendored Lucide/Feather license |
| Reference | `docs/archive/README.md` | Archive mapping and use |
| Historical Evidence | `CHANGELOG.md` | User-visible release history |
| Historical Evidence | `docs/archive/FROZEN-2026-09-10.md` | Accepted session and rail baseline |
| Historical Evidence | `docs/archive/QOL-20260913.md` | Bluetooth, badge, and readability freeze |
| Historical Evidence | `docs/archive/NOTIFICATION-HISTORY-ACTIONS-20260915.md` | Action API and popup investigation |
| Historical Evidence | `docs/archive/NOTIFICATION-PRESENTER-HANDOFF.md` | Deferred presenter ownership investigation |
| Historical Evidence | `docs/archive/T1-TICKER-GEOMETRY.md` | Ticker geometry correction record |
| Historical Evidence | `docs/archive/T1-TICKER-EVIDENCE.txt` | Raw ticker fixture output |

The legal and icon-license files are live distribution contracts. Keep them even
though they are excluded from documentation-size metrics.
