#!/usr/bin/env python3
"""Guard the live documents without interpreting archived evidence."""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DOC_INDEX = ROOT / "docs" / "README.md"
ARCHIVE_INDEX = ROOT / "docs" / "archive" / "README.md"
errors: list[str] = []


def tracked_documents() -> list[str]:
    result = subprocess.run(
        ["git", "ls-files", "--", "*.md", "*.txt"],
        cwd=ROOT,
        check=True,
        text=True,
        capture_output=True,
    )
    documents = []
    root_docs = {
        "AGENTS.md",
        "CHANGELOG.md",
        "CLAUDE.md",
        "README.md",
        "THIRD_PARTY_NOTICES.md",
        "TRADEMARKS.md",
    }
    for path in result.stdout.splitlines():
        if Path(path).name == "CMakeLists.txt" or path.startswith("LICENSES/"):
            continue
        if path in root_docs or path.startswith("docs/"):
            documents.append(path)
        elif path.endswith("/README.md") and path.split("/", 1)[0] in {
            "packaging",
            "patches",
            "tests",
        }:
            documents.append(path)
        elif path.endswith("/THIRD_PARTY.md"):
            documents.append(path)
    return documents


def index_mentions(path: str, text: str) -> bool:
    candidates = {path, Path(path).name}
    if path.startswith("docs/"):
        candidates.add(path.removeprefix("docs/"))
    else:
        candidates.add(f"../{path}")
    if Path(path).name == "README.md" and "/" in path and not path.startswith("docs/"):
        candidates.discard("README.md")
    return any(candidate in text for candidate in candidates)


def check_index_coverage() -> None:
    main_index = DOC_INDEX.read_text()
    # Archived evidence is kept privately; a checkout may carry none.
    archive_index = ARCHIVE_INDEX.read_text() if ARCHIVE_INDEX.exists() else ""
    for path in tracked_documents():
        if path.startswith("docs/archive/") and path != "docs/archive/README.md":
            if not index_mentions(path, archive_index):
                errors.append(f"archive index does not cover {path}")
        elif not index_mentions(path, main_index):
            errors.append(f"documentation index does not cover {path}")


def check_archive_authority() -> None:
    evidence_link = re.compile(
        r"(?:docs/)?archive/(?!README\.md\b)[^\s)`\]]+\.(?:md|txt)", re.IGNORECASE
    )
    for path in tracked_documents():
        if path.startswith("docs/archive/") or path == "docs/README.md":
            continue
        text = (ROOT / path).read_text()
        for match in evidence_link.finditer(text):
            errors.append(
                f"{path} treats archived evidence as a live reference: {match.group(0)}"
            )


# Live documents state what is true now; dated progress and commit references
# belong in Git history.
LIVE_DOCUMENTS = (
    "AGENTS.md",
    "CLAUDE.md",
    "docs/AMBIENT-BOUNDARY.md",
    "docs/ARCHITECTURE.md",
    "docs/INPUT.md",
    "docs/ROADMAP.md",
    "docs/README.md",
)


def check_live_hygiene() -> None:
    for path in LIVE_DOCUMENTS:
        text = (ROOT / path).read_text()
        if re.search(r"\b[0-9a-f]{7,40}\b", text, flags=re.IGNORECASE):
            errors.append(f"{path} contains a commit hash")
        if re.search(r"\b20\d{2}-\d{2}-\d{2}\b", text):
            errors.append(f"{path} contains dated progress")


# A document over its budget is trimmed; removed text lives in Git history. A
# budget is raised only with the maintainer's agreement.
WORD_BUDGETS = {
    "AGENTS.md": 205,
    "CLAUDE.md": 60,
    "README.md": 840,
    "docs/AMBIENT-BOUNDARY.md": 295,
    "docs/ARCHITECTURE.md": 1875,
    "docs/INPUT.md": 1350,
    "docs/ROADMAP.md": 270,
    "docs/README.md": 215,
    "packaging/README.md": 235,
}


def check_word_budgets() -> None:
    for path, budget in WORD_BUDGETS.items():
        words = len((ROOT / path).read_text().split())
        if words > budget:
            errors.append(f"{path} has {words} words; budget is {budget}")


def check_input_shape() -> None:
    """INPUT.md rows are copied verbatim into a product-wide list, so its shape
    is fixed: a short intro and the controls map, then a two-column table per
    destination and per subsection under it. tests/verify-public.py checks the
    map itself."""
    path = "docs/INPUT.md"
    text = (ROOT / path).read_text()
    parts = re.split(r"(?m)^###? ", text)
    intro = re.sub(r"(?m)^(# .*|\|.*)$", "", parts[0]).strip()
    sentences = len(re.findall(r"[.!?](?:\s|$)", intro))
    if not 2 <= sentences <= 3:
        errors.append(f"{path} intro has {sentences} sentences; expected 2 or 3")
    if len(parts) < 2:
        errors.append(f"{path} has no surface sections")
    for section in parts[1:]:
        title, _, body = section.partition("\n")
        rows = [line.strip() for line in body.splitlines() if line.strip()]
        if rows[:2] != ["| Input | What happens |", "| --- | --- |"]:
            errors.append(f"{path} § {title} does not start with the Input table")
            continue
        for row in rows[2:]:
            cells = row.strip("|").split(" | ")
            if not row.startswith("| ") or not row.endswith(" |") or len(cells) != 2:
                errors.append(f"{path} § {title} has a row that is not two cells: {row}")


check_index_coverage()
check_archive_authority()
check_live_hygiene()
check_word_budgets()
check_input_shape()

if errors:
    for error in errors:
        print(f"documentation guard: {error}", file=sys.stderr)
    raise SystemExit(1)

print("Documentation hygiene checks passed.")
