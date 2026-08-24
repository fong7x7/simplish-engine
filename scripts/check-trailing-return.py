#!/usr/bin/env python3
"""Check for trailing return types on function declarations/definitions.

Per project convention, prefer standard (leading) return types over
trailing return types (``auto foo() -> int`` should be ``int foo()``).

Lambdas and C++17 deduction guides are excluded since they require
the trailing syntax.

Output format: file:line: [trailing-return] message
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
    extract_function_name_from_sig_lines,
    multiline_raw_string_opens,
    strip_strings_and_line_comment,
)
from find_sources import discover_files, parse_common_args

_LAMBDA_CAPTURE_RE = re.compile(r"\[.*\]\s*\(")

_AUTO_KW_RE = re.compile(r"\bauto\b")

_TRAILING_ARROW_RE = re.compile(
    r"\)\s*"
    r"(?:const\s*)?"
    r"(?:[&]{1,2}\s*)?"
    r"(?:volatile\s*)?"
    r"(?:noexcept(?:\([^)]*\))?\s*)?"
    r"(?:(?:override|final)\s*)?"
    r"->\s*(.+)",
)


def _line_has_lambda_arrow(masked: str) -> bool:
    """True if the ``->`` on this line belongs to a lambda expression."""
    return bool(_LAMBDA_CAPTURE_RE.search(masked))


_FUNC_NAME_RE = re.compile(r"(\w+)\s*\(")

_SKIP_NAMES = frozenset({
    "if", "for", "while", "switch", "case", "catch", "throw",
    "return", "sizeof", "decltype", "static_assert",
    "auto", "template",
})


def _extract_name_from_line(masked: str) -> str:
    """Extract the function name from a single line containing ``auto name(``."""
    for m in _FUNC_NAME_RE.finditer(masked):
        name = m.group(1)
        if name not in _SKIP_NAMES:
            return name
    return "<unknown>"


def _extract_return_type(after_arrow: str) -> str:
    """Pull the return type token(s) from text after ``->``."""
    text = after_arrow.strip()
    result: list[str] = []
    angle_depth = 0
    for ch in text:
        if ch in ("{", ";"):
            break
        if ch == "<":
            angle_depth += 1
        elif ch == ">":
            angle_depth -= 1
        if angle_depth < 0:
            break
        result.append(ch)
    return "".join(result).strip().rstrip("{; \t")


def check_file(filepath: Path, lines: list[str] | None = None) -> list[str]:
    """Check a single file for trailing return type violations."""
    violations: list[str] = []
    if lines is None:
        try:
            lines = filepath.read_text(
                encoding="utf-8", errors="replace",
            ).splitlines()
        except OSError:
            return violations

    in_block_comment = False
    raw_delim: str | None = None

    for i, line in enumerate(lines):
        line_text, in_block_comment = advance_past_block_comment(
            line, in_block_comment,
        )
        if in_block_comment:
            continue

        line_text, raw_delim = advance_past_multiline_raw_close(
            line_text, raw_delim,
        )
        if raw_delim is not None:
            continue

        stripped = line_text.strip()
        if not stripped or stripped.startswith("//") or stripped.startswith("#"):
            continue

        new_raw = multiline_raw_string_opens(line_text)
        if new_raw is not None:
            raw_delim = new_raw
            continue

        if "NOLINT" in line:
            continue

        masked = strip_strings_and_line_comment(line_text)

        if "->" not in masked:
            continue

        if _line_has_lambda_arrow(masked):
            continue

        if not _AUTO_KW_RE.search(masked):
            continue

        if ")" not in masked:
            continue

        arrow_pos = masked.find("->")
        auto_pos = masked.find("auto")
        eq_pos = masked.find("=")
        if eq_pos != -1 and auto_pos < eq_pos < arrow_pos:
            continue

        m = _TRAILING_ARROW_RE.search(masked)
        if not m:
            continue

        return_type = _extract_return_type(m.group(1))
        if not return_type:
            continue

        name = _extract_name_from_line(masked)
        if name == "<unknown>":
            sig_lines = lines[max(0, i - 1) : i + 1]
            name = extract_function_name_from_sig_lines(sig_lines)
        if name == "<unknown>":
            continue

        violations.append(
            f"{filepath}:{i + 1}: [trailing-return] "
            f"function '{name}' uses trailing return type '-> {return_type}' "
            f"-- use standard return type instead"
        )

    return violations


def main() -> int:
    args = parse_common_args(
        "Check for trailing return types on function definitions",
    )
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
        print(f"\n{total} trailing-return violation(s) found.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
