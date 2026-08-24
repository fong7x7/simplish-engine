#!/usr/bin/env python3
"""Check for C++ exception keywords (throw, try, catch).

Per CLAUDE.md: "No C++ exceptions (-fno-exceptions)."
Excludes Catch2 test macros, comment lines, and string literals.

Output format: file:line: [no-exceptions] message
Exit code: non-zero if any violations found.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "lib"))

from cpp_scan_utils import strip_strings_and_line_comment
from find_sources import discover_files, parse_common_args

_EXCEPTION_KW_RE = re.compile(r"\b(throw|try|catch)\b")

_CATCH2_RE = re.compile(
    r"CATCH2|INTERNAL_CATCH|TEST_CASE|SECTION|REQUIRE|CHECK|BENCHMARK"
    r"|SCENARIO|GIVEN|WHEN|THEN|AND_GIVEN|AND_WHEN|AND_THEN",
)


def _is_skippable(stripped: str) -> bool:
    """Lines that cannot contain real exception keywords."""
    if not stripped:
        return True
    first = stripped[0]
    if first in ("*", "#"):
        return True
    return stripped.startswith("//") or stripped.startswith("/*")


def check_file(filepath: Path, lines: list[str] | None = None) -> list[str]:
    """Check a single file for C++ exception keyword violations."""
    violations: list[str] = []
    if lines is None:
        try:
            lines = filepath.read_text(
                encoding="utf-8", errors="replace",
            ).splitlines()
        except OSError:
            return violations

    for i, line in enumerate(lines):
        stripped = line.strip()
        if _is_skippable(stripped):
            continue
        if _CATCH2_RE.search(line):
            continue
        if "NOLINT" in line:
            continue

        masked = strip_strings_and_line_comment(line)
        m = _EXCEPTION_KW_RE.search(masked)
        if m:
            violations.append(
                f"{filepath}:{i + 1}: [no-exceptions] "
                f"'{m.group(1)}' keyword found "
                f"-- use std::optional/std::expected",
            )
    return violations


def main() -> int:
    args = parse_common_args("Check for C++ exception keywords")
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
        print(f"\n{total} no-exceptions violation(s) found.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
