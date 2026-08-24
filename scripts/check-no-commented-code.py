#!/usr/bin/env python3
"""Check for commented-out code blocks (2+ consecutive lines).

Per CLAUDE.md: "No commented-out code."
Uses heuristics to distinguish code comments from prose comments.

Output format: file:start-end: [commented-code] message
Exit code: non-zero if any violations found.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "lib"))

from find_sources import discover_files, parse_common_args

_COMMENT_PREFIX_RE = re.compile(r"^\s*//\s?")

_NON_CODE_RE = re.compile(
    r"^(TODO|FIXME|NOTE|HACK|WARN|XXX|Req:|@|Copyright|License"
    r"|SPDX|NOLINT|---|\*\*|##)",
    re.IGNORECASE,
)
_URL_RE = re.compile(r"^(https?://|[A-Za-z]+://)", re.IGNORECASE)

_CODE_PATTERNS: list[re.Pattern[str]] = [
    re.compile(r";\s*$"),
    re.compile(
        r"^\s*(if|for|while|return|auto|int|void|float|double|bool|char"
        r"|unsigned|const|static|std::)\s",
    ),
    re.compile(r"^\s*\w+\.\w+\("),
    re.compile(r"^\s*\w+\s*=\s*"),
    re.compile(r"^\s*\w+\s*\(.*\)\s*\{?\s*$"),
    re.compile(r"^\s*[{}]\s*$"),
    re.compile(r"^\s*#(include|define|ifdef|ifndef|endif|pragma)"),
]


def _looks_like_code(line: str) -> bool:
    """Heuristic: does this ``//`` comment line contain code?"""
    content = _COMMENT_PREFIX_RE.sub("", line).strip()
    if not content:
        return False
    if line.strip().startswith("///"):
        return False
    if _NON_CODE_RE.match(content):
        return False
    if _URL_RE.match(content):
        return False
    return any(p.search(content) for p in _CODE_PATTERNS)


def _emit_violation(
    filepath: Path, start: int, end: int, count: int,
) -> str:
    return (
        f"{filepath}:{start}-{end}: [commented-code] "
        f"{count} consecutive lines of commented-out code"
    )


def check_file(filepath: Path, lines: list[str] | None = None) -> list[str]:
    """Check a single file for commented-out code blocks."""
    violations: list[str] = []
    if lines is None:
        try:
            lines = filepath.read_text(
                encoding="utf-8", errors="replace",
            ).splitlines()
        except OSError:
            return violations

    consecutive = 0
    start_line = 0

    for i, raw_line in enumerate(lines):
        stripped = raw_line.strip()
        is_comment = stripped.startswith("//") and not stripped.startswith("///")

        if is_comment and _looks_like_code(raw_line):
            if consecutive == 0:
                start_line = i + 1
            consecutive += 1
        else:
            if consecutive >= 2:
                violations.append(
                    _emit_violation(filepath, start_line, i, consecutive),
                )
            consecutive = 0

    if consecutive >= 2:
        violations.append(
            _emit_violation(filepath, start_line, len(lines), consecutive),
        )
    return violations


def main() -> int:
    args = parse_common_args("Check for commented-out code blocks")
    files = discover_files(
        paths=args.paths or None,
        src_only=args.src_only,
        tests_only=args.tests_only,
    )
    all_files = files.all_files
    if not all_files:
        print("No C++ files to check.")
        return 0

    total = 0
    for f in all_files:
        for msg in check_file(f):
            print(msg)
            total += 1

    if total > 0:
        print(f"\n{total} commented-code violation(s) found.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
