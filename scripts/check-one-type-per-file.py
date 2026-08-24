#!/usr/bin/env python3
"""Check that each header file declares exactly one top-level struct or class.

Forward declarations and nested types are excluded. Enums colocated with
the primary type are allowed per CLAUDE.md.

Output format: file:line: [one-type-per-file] message
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
)
from find_sources import discover_files, parse_common_args

# Matches struct/class definitions (not forward declarations).
# Forward declarations end with ';' on the same line and have no '{'.
TYPE_DEF_RE = re.compile(
    r"^\s*(?:template\s*<[^>]*>\s*)?(struct|class)\s+(\w+)"
)


def check_file(filepath: Path, lines: list[str] | None = None) -> list[str]:
    """Check a single header file for multiple top-level type definitions."""
    violations: list[str] = []
    if lines is None:
        try:
            lines = filepath.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            return violations

    brace_depth = 0
    namespace_depth = 0
    in_block_comment = False
    raw_delim: str | None = None

    # Track top-level type definitions: (name, line_number)
    top_level_types: list[tuple[str, int]] = []

    i = 0
    while i < len(lines):
        line = lines[i]
        line, in_block_comment = advance_past_block_comment(line, in_block_comment)
        if in_block_comment:
            i += 1
            continue
        line, raw_delim = advance_past_multiline_raw_close(line, raw_delim)
        if raw_delim is not None:
            i += 1
            continue
        stripped = line.strip()
        if not stripped:
            i += 1
            continue
        if stripped.startswith("/*") and "*/" not in stripped:
            in_block_comment = True
            i += 1
            continue

        new_raw = multiline_raw_string_opens(line)
        if new_raw is not None:
            raw_delim = new_raw
            i += 1
            continue

        # Skip single-line comments and preprocessor
        if stripped.startswith("//") or stripped.startswith("#"):
            i += 1
            continue

        # Check for namespace (we consider types at namespace level as "top-level")
        ns_match = re.match(r"\s*namespace\s+\w+", line)
        if ns_match and "{" in stripped:
            namespace_depth += 1

        # Check for type definitions at the top level (brace_depth == namespace_depth)
        type_match = TYPE_DEF_RE.match(line)
        if type_match and brace_depth == namespace_depth:
            name = type_match.group(2)

            # Check if this is a forward declaration (no '{' on this or next line)
            is_forward = False
            if ";" in stripped and "{" not in stripped:
                is_forward = True
            elif "{" not in stripped:
                # Check next non-empty line
                for k in range(i + 1, min(i + 5, len(lines))):
                    ns = lines[k].strip()
                    if not ns or ns.startswith("//"):
                        continue
                    if ns.startswith("{") or ns == "{":
                        break
                    if ";" in ns and "{" not in ns:
                        is_forward = True
                    break

            if not is_forward:
                # Honor NOLINT(one-type-per-file) suppression comments
                if "NOLINT" in stripped and "one-type-per-file" in stripped:
                    i += 1
                    # Still track brace depth for the skipped line
                    for ch in stripped:
                        if ch == "{":
                            brace_depth += 1
                        elif ch == "}":
                            brace_depth -= 1
                            if brace_depth < namespace_depth:
                                namespace_depth = brace_depth
                    continue
                top_level_types.append((name, i + 1))

        # Track brace depth
        for ch in stripped:
            if ch == "{":
                brace_depth += 1
            elif ch == "}":
                brace_depth -= 1
                if brace_depth < namespace_depth:
                    namespace_depth = brace_depth

        i += 1

    if len(top_level_types) > 1:
        names = ", ".join(t[0] for t in top_level_types)
        first_line = top_level_types[0][1]
        violations.append(
            f"{filepath}:{first_line}: [one-type-per-file] "
            f"header declares {len(top_level_types)} top-level types: "
            f"{names} (limit: 1)"
        )

    return violations


def main() -> int:
    args = parse_common_args("Check one struct/class per header file")
    files = discover_files(
        paths=args.paths or None,
        src_only=args.src_only,
        tests_only=args.tests_only,
    )

    # Only check header files
    headers = files.all_headers
    if not headers:
        print("No header files to check.")
        return 0

    total_violations = 0
    for f in headers:
        violations = check_file(f)
        for v in violations:
            print(v)
            total_violations += 1

    if total_violations > 0:
        print(f"\n{total_violations} one-type-per-file violation(s) found.")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
