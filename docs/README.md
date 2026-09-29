# Documentation

Each document owns one subject and states what is true now; removed text lives in
Git history. `tests/verify-docs.py` fails when a tracked document is missing from
this table, so add a new one here in the same change.

| Document | Owns |
|---|---|
| `AGENTS.md` | Startup set and working rules |
| `CLAUDE.md` | Claude Code entry point into `AGENTS.md` |
| `README.md` | What Temperance is, compatibility, build and settings |
| `docs/ARCHITECTURE.md` | How Temperance works and why |
| `docs/INPUT.md` | Every touch, click and key it answers |
| `docs/ROADMAP.md` | What is planned |
| `docs/AMBIENT-BOUNDARY.md` | The split with Ambient Tettegouche |
| `docs/README.md` | This table |
| `CHANGELOG.md` | User-visible changes by release |
| `packaging/README.md` | Installing and removing a release |
| `THIRD_PARTY_NOTICES.md` | Runtime service and derived-work notices |
| `TRADEMARKS.md` | The project names and marks |
| `assets/icons/THIRD_PARTY.md` | Icon sources and rendering |
| `LICENSES/`, `assets/icons/lucide/LICENSE` | Licence texts shipped with the code |
