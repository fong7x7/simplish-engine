#!/usr/bin/env python3
"""Check for forward declarations in headers.

Per project convention: include the correct header for needed types
instead of forward-declaring them. Forward declarations create fragile
coupling and hide real dependencies.

Detects patterns like:
  class Foo;
  struct Bar;
  enum class Baz;
  enum struct Qux;
  enum class Baz : uint8_t;
  class ENGINE_API Foo;

Excludes:
  - Lines inside block comments or raw strings
  - Comment-only lines
  - String literals
  - NOLINT-suppressed lines
  - Template forward declarations (template<...> class Foo;)

Output format: file:line: [no-forward-decl] message
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

_FORWARD_DECL_RE = re.compile(
    r"^\s*"
    r"(?:enum\s+(?:class|struct)\s+\w+(?:\s*:\s*\w+(?:::\w+)*)?"
    r"|(?:class|struct)\s+(?:[A-Z_][A-Z0-9_]*\s+)?\w+)"
    r"\s*;",
)

_TEMPLATE_RE = re.compile(r"^\s*template\s*<")


def check_file(filepath: Path, lines: list[str] | None = None) -> list[str]:
    """Check a single file for forward declaration violations."""
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
    prev_was_template = False

    for i, line in enumerate(lines):
        line_to_scan, in_block_comment = advance_past_block_comment(
            line, in_block_comment,
        )
        if not line_to_scan and in_block_comment:
            prev_was_template = False
            continue

        line_to_scan, raw_delim = advance_past_multiline_raw_close(
            line_to_scan, raw_delim,
        )
        if raw_delim is not None:
            prev_was_template = False
            continue

        new_raw = multiline_raw_string_opens(line_to_scan)
        if new_raw is not None:
            raw_delim = new_raw
            prev_was_template = False
            continue

        stripped = line.strip()
        if not stripped or stripped.startswith("//") or stripped.startswith("/*"):
            prev_was_template = False
            continue

        if "NOLINT" in line:
            prev_was_template = False
            continue

        if _TEMPLATE_RE.match(stripped):
            prev_was_template = True
            continue

        masked = strip_strings_and_line_comment(line)
        m = _FORWARD_DECL_RE.match(masked)
        if m:
            decl = stripped.rstrip(";").strip()
            if prev_was_template:
                prev_was_template = False
                continue
            violations.append(
                f"{filepath}:{i + 1}: [no-forward-decl] "
                f"forward declaration '{decl}' "
                f"-- include the correct header instead",
            )
        prev_was_template = False

    return violations


def main() -> int:
    args = parse_common_args("Check for forward declarations")
    files = discover_files(
        paths=args.paths or None,
        src_only=args.src_only,
        tests_only=args.tests_only,
    )
    # Only headers -- forward declarations in .cpp files are acceptable.
    all_files = files.all_headers
    if not all_files:
        print("No header files to check.")
        return 0

    total = 0
    for f in all_files:
        for msg in check_file(f):
            print(msg)
            total += 1

    if total > 0:
        print(f"\n{total} no-forward-decl violation(s) found.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
