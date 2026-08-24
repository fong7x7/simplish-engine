#!/usr/bin/env python3
"""Check for bare new/delete outside smart pointers and string-literal noise.

Output format: file:line: [bare-new-delete] message
Exit code: non-zero if any violations found.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "lib"))

from cpp_scan_utils import (
    advance_past_block_comment,
    advance_past_multiline_raw_close,
    multiline_raw_string_opens,
    strip_strings_and_line_comment,
)
from find_sources import discover_files, parse_common_args

# Allocation: new T / new (not placement new (...)
NEW_ALLOC_RE = re.compile(r"\bnew\b(?!\s*\()")
DELETE_RE = re.compile(r"\bdelete\b")


def _line_is_comment_only(stripped: str) -> bool:
    return bool(
        stripped.startswith("//")
        or stripped.startswith("*")
        or stripped.startswith("/*")
    )


def _masked_line_violates_new(original: str, masked: str) -> bool:
    if not NEW_ALLOC_RE.search(masked):
        return False
    if re.search(r"make_unique|make_shared|std::make_", original):
        return False
    if re.search(r"operator\s+new\b", original):
        return False
    if "NOLINT" in original:
        return False
    return True


def _line_violates_delete(original: str, masked: str) -> bool:
    if not DELETE_RE.search(masked):
        return False
    if re.search(r"operator\s+delete\b", original):
        return False
    if re.search(r"=\s*delete\b", original):
        return False
    if "NOLINT" in original:
        return False
    return True


def check_file(filepath: Path, lines: list[str] | None = None) -> list[str]:
    violations: list[str] = []
    if lines is None:
        try:
            lines = filepath.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            return violations

    raw_delim: str | None = None
    in_block_comment = False
    for i, line in enumerate(lines):
        line, in_block_comment = advance_past_block_comment(line, in_block_comment)
        if in_block_comment:
            continue

        line, raw_delim = advance_past_multiline_raw_close(line, raw_delim)
        if raw_delim is not None:
            continue

        stripped = line.strip()
        if not stripped:
            continue

        if stripped.startswith("/*"):
            if "*/" not in stripped:
                in_block_comment = True
            continue

        new_raw = multiline_raw_string_opens(line)
        if new_raw is not None:
            raw_delim = new_raw
            continue

        if _line_is_comment_only(stripped):
            continue
        if stripped.startswith("#"):
            continue

        masked = strip_strings_and_line_comment(line)
        if _masked_line_violates_new(line, masked):
            violations.append(
                f"{filepath}:{i + 1}: [bare-new-delete] "
                f"bare 'new' found -- use std::make_unique or std::make_shared"
            )
        if _line_violates_delete(line, masked):
            violations.append(
                f"{filepath}:{i + 1}: [bare-new-delete] "
                f"bare 'delete' found -- use RAII / smart pointers"
            )

    return violations


def main() -> int:
    args = parse_common_args("Check for bare new/delete")
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
        print(f"\n{total} bare-new-delete violation(s) found.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
