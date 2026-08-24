#!/usr/bin/env python3
"""Check that function bodies do not exceed 16 non-blank, non-comment lines.

Named algorithms (preceded by a comment header like '// Algorithm:' or a
block comment) are allowed up to 50 lines.

Output format: file:line: [function-size] message
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
    is_full_line_comment,
    multiline_raw_string_opens,
    strip_strings_and_line_comment,
)
from find_sources import discover_files, parse_common_args

# Heuristic: lines that start a function definition.
# Matches return-type + name + '(' but NOT control-flow, type defs, or macros.
CONTROL_FLOW = {"if", "else", "for", "while", "do", "switch", "case", "catch", "try"}
TYPE_DEF_KW = {"struct", "class", "enum", "namespace", "union", "typedef", "using"}
# Lines starting with `template` declare templates (class/function/alias), not a plain function body.
TEMPLATE_KW = "template"

# Algorithm header: a comment line immediately before the function containing
# keywords like "Algorithm", "---", or a block description.
ALGO_HEADER_RE = re.compile(
    r"^\s*/(/|\*).*("
    r"[Aa]lgorithm|[Nn]amed algorithm|ALGORITHM"
    r"|-{3,}"
    r"|@algorithm"
    r")",
)


def _is_countable_line(line: str) -> bool:
    """Return True if this line counts toward the body size limit."""
    stripped = line.strip()
    if not stripped:
        return False
    if stripped.startswith("//"):
        return False
    if stripped.startswith("/*") and stripped.endswith("*/"):
        return False
    return True


def _first_token(line: str) -> str:
    """Return the first whitespace-delimited token on the line."""
    stripped = line.strip()
    if not stripped:
        return ""
    return stripped.split()[0].rstrip("(")


def _signature_combined_until_open_paren(prev_lines: list[str], current: str) -> str:
    """Build signature text upward until '(' appears; skips full-line comments."""
    combined = "" if is_full_line_comment(current) else current.strip()
    for pl in reversed(prev_lines[-5:]):
        if is_full_line_comment(pl):
            continue
        ps = pl.strip()
        combined = ps + " " + combined
        if "(" in combined:
            break
    return combined.strip()


def _first_code_token_from_combined(combined: str) -> str:
    """First meaningful token from combined signature text (skips braces, comments)."""
    stripped = combined.strip()
    while stripped.startswith("//"):
        nl = stripped.find("\n")
        if nl == -1:
            return ""
        stripped = stripped[nl + 1 :].strip()
    for raw in stripped.split():
        tok = raw.rstrip(";:,.(){}")
        if not tok or tok.startswith("//"):
            continue
        if tok in ("{", "}"):
            continue
        return tok
    return ""


def _looks_like_function_def(line: str, prev_lines: list[str]) -> bool:
    """Heuristic: does this line (ending with '{') look like a function definition?"""
    stripped = line.strip()
    # Must end with '{' (possibly after const/override/noexcept/final/=default)
    if not stripped.endswith("{"):
        return False
    # Skip single-brace lines (opening brace on its own line) — handled by caller
    # Skip preprocessor
    if stripped.startswith("#"):
        return False
    # Skip type definitions and control flow
    first = _first_token(stripped)
    if first == TEMPLATE_KW or first in TYPE_DEF_KW or first in CONTROL_FLOW:
        return False
    # Skip lambdas assigned to variables: auto x = [...](...){
    if "= [" in stripped or "=[" in stripped:
        return False
    # Must contain '(' somewhere (function params) — either on this line or
    # in recent preceding lines (multi-line signature)
    combined = _signature_combined_until_open_paren(prev_lines, line)
    if "(" not in combined:
        return False
    # Skip if the combined text starts with a type-def / template keyword
    cfirst = _first_code_token_from_combined(combined)
    if cfirst == TEMPLATE_KW or cfirst in TYPE_DEF_KW or cfirst in CONTROL_FLOW:
        return False
    return True


def _has_algo_header(lines: list[str], func_start: int) -> bool:
    """Check if the lines preceding func_start contain an algorithm header.

    Skips multi-line function signatures (parameter lines ending with `,`) so
    ``// Algorithm:`` or ``/// ... algorithm`` above the signature is still
    found. Looks back up to 24 lines.
    """
    for i in range(func_start - 1, max(func_start - 24, -1), -1):
        if i < 0:
            break
        stripped = lines[i].strip()
        if not stripped:
            continue
        if ALGO_HEADER_RE.match(lines[i]):
            return True
        if is_full_line_comment(lines[i]) and re.search(
            r"[Aa]lgorithm|@algorithm", lines[i]
        ):
            return True
        if is_full_line_comment(lines[i]):
            continue
        if stripped.endswith(","):
            continue
        if "TEST_CASE" in stripped:
            continue
        if stripped.startswith('"'):
            continue
        if (
            "<" in stripped
            and ">" in stripped
            and ";" not in stripped
            and "{" not in stripped
        ):
            continue
        break
    return False


def check_file(filepath: Path, lines: list[str] | None = None) -> list[str]:
    """Check a single file for function size violations.

    Returns list of violation messages.
    """
    violations: list[str] = []
    if lines is None:
        try:
            lines = filepath.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            return violations

    i = 0
    in_block_comment = False
    raw_delim: str | None = None
    prev_lines: list[str] = []

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
        if not line.strip():
            i += 1
            continue
        stripped = line.strip()
        if stripped.startswith("/*") and "*/" not in stripped:
            in_block_comment = True
            i += 1
            continue

        new_raw = multiline_raw_string_opens(line)
        if new_raw is not None:
            raw_delim = new_raw
            i += 1
            continue

        # Look for function opening: a line ending with '{' that looks like a
        # function definition, OR a standalone '{' after a function signature.
        is_func = False
        func_line = i

        if stripped == "{" and prev_lines:
            # Standalone opening brace — check if previous non-blank lines
            # form a function signature (contain '(' and ')')
            combined = ""
            for pl in reversed(prev_lines[-5:]):
                ps = pl.strip()
                if not ps:
                    break
                if ps.startswith("//"):
                    continue
                combined = ps + " " + combined
                if "(" in combined and ")" in combined:
                    cfirst = _first_code_token_from_combined(combined)
                    if (
                        cfirst
                        and cfirst != TEMPLATE_KW
                        and cfirst not in TYPE_DEF_KW
                        and cfirst not in CONTROL_FLOW
                    ):
                        is_func = True
                    break
        elif stripped.endswith("{") and len(stripped) > 1:
            is_func = _looks_like_function_def(line, prev_lines)

        if is_func:
            # Count the function body
            brace_depth = 1
            body_start = i + 1
            countable = 0
            j = body_start
            body_in_block_comment = False
            body_raw_delim: str | None = None

            while j < len(lines) and brace_depth > 0:
                bline = lines[j]
                bline, body_in_block_comment = advance_past_block_comment(
                    bline, body_in_block_comment
                )
                if body_in_block_comment:
                    j += 1
                    continue
                bline, body_raw_delim = advance_past_multiline_raw_close(
                    bline, body_raw_delim
                )
                if body_raw_delim is not None:
                    j += 1
                    continue
                if not bline.strip():
                    j += 1
                    continue
                bstripped = bline.strip()
                if bstripped.startswith("/*") and "*/" not in bstripped:
                    body_in_block_comment = True
                    j += 1
                    continue

                body_new_raw = multiline_raw_string_opens(bline)
                if body_new_raw is not None:
                    body_raw_delim = body_new_raw
                    j += 1
                    continue

                # Track braces outside string literals (raw JSON in tests contains
                # { } that must not affect nesting depth).
                brace_scan = strip_strings_and_line_comment(bline)
                for ch in brace_scan:
                    if ch == "{":
                        brace_depth += 1
                    elif ch == "}":
                        brace_depth -= 1
                        if brace_depth == 0:
                            break

                if brace_depth > 0 and _is_countable_line(bline):
                    countable += 1

                j += 1

            # Determine limit
            is_algo = _has_algo_header(lines, func_line)
            limit = 50 if is_algo else 16

            if countable > limit:
                # Try to extract function name
                sig_lines = lines[max(func_line - 3, 0) : i + 1]
                name = extract_function_name_from_sig_lines(sig_lines)
                kind = "named algorithm" if is_algo else "function"

                violations.append(
                    f"{filepath}:{func_line + 1}: [function-size] "
                    f"{kind} '{name}' has {countable} non-blank lines "
                    f"(limit: {limit})"
                )

            # Skip past the function body
            i = j
            prev_lines = []
            continue

        prev_lines.append(line)
        if len(prev_lines) > 10:
            prev_lines = prev_lines[-10:]
        i += 1

    return violations


def main() -> int:
    args = parse_common_args("Check function body size limits (16 / 50 lines)")
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
        print(f"\n{total_violations} function-size violation(s) found.")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
