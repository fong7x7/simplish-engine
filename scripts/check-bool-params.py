#!/usr/bin/env python3
"""Check that no function uses bare 'bool' parameters.

Per CLAUDE.md: "Bool params banned: Use enum class with two values instead."

Output format: file:line: [bool-param] message
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
)
from find_sources import discover_files, parse_common_args

# Non-function keywords
NON_FUNC_KW = {
    "if", "else", "for", "while", "do", "switch", "case", "catch", "try",
    "template",
    "return", "throw", "co_return", "co_await", "co_yield",
}

# Macros to skip
MACRO_PREFIXES = (
    "TEST_CASE", "SECTION", "REQUIRE", "CHECK", "BENCHMARK",
    "ENGINE_ASSERT", "ENGINE_EVENT_TRAIT", "ENG_", "CATCH_",
    "INTERNAL_CATCH", "TEMPLATE_TEST_CASE",
    "GENERATE", "STATIC_REQUIRE", "INFO", "WARN", "FAIL",
    "SCENARIO", "GIVEN", "WHEN", "THEN", "AND_GIVEN", "AND_WHEN", "AND_THEN",
)

# Pattern to find 'bool' as a parameter type in a parameter list
# Matches: bool name, const bool name, const bool& name, bool& name
BOOL_PARAM_RE = re.compile(
    r"(?:^|,)\s*(?:const\s+)?bool\s*[&*]?\s+(\w+)"
)


def _is_macro(name: str) -> bool:
    if name.isupper() or name.startswith("ENGINE_"):
        return True
    for prefix in MACRO_PREFIXES:
        if name.startswith(prefix):
            return True
    return False


def check_file(filepath: Path, lines: list[str] | None = None) -> list[str]:
    """Check a single file for bool parameter violations."""
    violations: list[str] = []
    if lines is None:
        try:
            lines = filepath.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            return violations

    in_block_comment = False
    raw_delim: str | None = None
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

        # Skip comments and preprocessor
        if stripped.startswith("//") or stripped.startswith("#"):
            i += 1
            continue

        # Need a '(' to potentially be a function
        if "(" not in stripped:
            i += 1
            continue

        # Skip non-function keywords
        first_token = stripped.split()[0].rstrip("(") if stripped else ""
        if first_token in NON_FUNC_KW:
            i += 1
            continue

        # Accumulate multi-line parameter lists
        combined = stripped
        paren_depth = 0
        found_open = False
        close_found = False

        for ch in combined:
            if ch == "(":
                if not found_open:
                    found_open = True
                paren_depth += 1
            elif ch == ")":
                paren_depth -= 1
                if paren_depth == 0 and found_open:
                    close_found = True
                    break

        j = i + 1
        while not close_found and j < len(lines) and j < i + 10:
            next_line = lines[j].strip()
            combined += " " + next_line
            for ch in next_line:
                if ch == "(":
                    if not found_open:
                        found_open = True
                    paren_depth += 1
                elif ch == ")":
                    paren_depth -= 1
                    if paren_depth == 0 and found_open:
                        close_found = True
                        break
            j += 1

        if not close_found or not found_open:
            i += 1
            continue

        sig_lines = lines[max(0, i - 3) : j]
        name = extract_function_name_from_sig_lines(sig_lines)
        if name == "<unknown>" or _is_macro(name):
            i += 1
            continue

        # Skip operator overloads
        if name == "operator":
            i += 1
            continue

        # Extract parameter string
        open_idx = combined.index("(")
        depth = 0
        close_idx = -1
        for ci, ch in enumerate(combined[open_idx:], start=open_idx):
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    close_idx = ci
                    break

        if close_idx < 0:
            i += 1
            continue

        param_str = combined[open_idx + 1 : close_idx].strip()
        if not param_str or param_str == "void":
            i += 1
            continue

        # Check for bool parameters
        bool_matches = BOOL_PARAM_RE.findall(param_str)
        if bool_matches:
            param_names = ", ".join(f"'{p}'" for p in bool_matches)
            violations.append(
                f"{filepath}:{i + 1}: [bool-param] "
                f"function '{name}' has bool parameter {param_names} "
                f"-- use enum class instead"
            )

        i += 1

    return violations


def main() -> int:
    args = parse_common_args("Check for banned bool function parameters")
    files = discover_files(
        paths=args.paths or None,
        src_only=args.src_only,
        tests_only=args.tests_only,
    )

    all_files = files.all_files
    if not all_files:
        print("No C++ files to check.")
        return 0

    total_violations = 0
    for f in all_files:
        violations = check_file(f)
        for v in violations:
            print(v)
            total_violations += 1

    if total_violations > 0:
        print(f"\n{total_violations} bool-param violation(s) found.")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
