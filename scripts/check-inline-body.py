#!/usr/bin/env python3
"""Check that header-defined function bodies contain at most one operation.

Multi-operation functions should be defined in .cpp files, not inline in
headers.  Template, constexpr, and consteval functions are exempt (they
must live in headers).

Counts "operations" as top-level statements in the function body:
  - Each ';' at brace depth 0 / paren depth 0 inside the body.
  - Each '}' that closes a compound block (if/for/while/switch) back to
    the body's top level.

Output format: file:line: [inline-body] message
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

_CONTROL_FLOW = frozenset({
    "if", "else", "for", "while", "do", "switch", "case", "catch", "try",
})
_TYPE_DEF_KW = frozenset({
    "struct", "class", "enum", "namespace", "union", "typedef", "using",
})

MAX_INLINE_OPS = 1


# -------------------------------------------------------------------
# Helpers
# -------------------------------------------------------------------

def _first_token(line: str) -> str:
    """First whitespace-delimited token, stripping template/paren noise."""
    stripped = line.strip()
    if not stripped:
        return ""
    return stripped.split()[0].split("<")[0].rstrip("(")


def _is_template_context(sig_text: str, prev_lines: list[str]) -> bool:
    """True if the function lives inside a template declaration."""
    if re.search(r"\btemplate\s*<", sig_text):
        return True
    for pl in reversed(prev_lines[-5:]):
        s = pl.strip()
        if not s or s.startswith("//") or s.startswith("/*"):
            continue
        return bool(re.match(r"\s*template\s*<", s))
    return False


def _sig_is_exempt(sig_text: str) -> bool:
    """True for constexpr / consteval signatures."""
    return "constexpr " in sig_text or "consteval " in sig_text


def _build_sig_text(prev_lines: list[str], current: str) -> str:
    """Combine recent context lines with the current line."""
    parts: list[str] = []
    for pl in prev_lines[-5:]:
        s = pl.strip()
        if s and not s.startswith("//") and not s.startswith("/*"):
            parts.append(s)
    parts.append(current.strip())
    return " ".join(parts)


def _looks_like_func(stripped: str) -> bool:
    """Quick filter: could this line be part of a function definition?"""
    if not stripped or stripped.startswith("#") or stripped.startswith("//"):
        return False
    ft = _first_token(stripped)
    if ft in _CONTROL_FLOW or ft in _TYPE_DEF_KW:
        return False
    if "= [" in stripped or "=[" in stripped or stripped.startswith("["):
        return False
    return "(" in stripped


# -------------------------------------------------------------------
# Statement counting
# -------------------------------------------------------------------

def _preceded_by_ctrl(text: str, brace_idx: int) -> bool:
    """True if ``{`` at *brace_idx* follows a control-flow close or keyword."""
    before = text[:brace_idx].rstrip()
    if before.endswith(")"):
        return True
    return any(before.endswith(kw) for kw in ("else", "do", "try"))


def _count_body_ops(chars: str) -> int:
    """Count top-level operations in already-string-stripped body text.

    ``chars`` is everything between the function's outer ``{`` and ``}``.
    Counts ``;`` at depth 0, plus ``}`` closings of control-flow blocks.
    Brace-initialiser ``}`` closings are NOT counted.
    """
    depth = 0
    paren = 0
    count = 0
    ctrl_stack: list[bool] = []
    just_closed_ctrl = False
    for ci, ch in enumerate(chars):
        if ch == "(":
            paren += 1
        elif ch == ")":
            paren = max(paren - 1, 0)
        elif ch == "{" and paren == 0:
            is_ctrl = _preceded_by_ctrl(chars, ci) if depth == 0 else False
            ctrl_stack.append(is_ctrl)
            depth += 1
        elif ch == "}" and paren == 0:
            depth = max(depth - 1, 0)
            was_ctrl = ctrl_stack.pop() if ctrl_stack else False
            if depth == 0 and was_ctrl:
                count += 1
                just_closed_ctrl = True
        elif ch == ";" and depth == 0 and paren == 0:
            if just_closed_ctrl:
                just_closed_ctrl = False
            else:
                count += 1
        elif ch not in " \t\n\r":
            just_closed_ctrl = False
    return count


def _oneliner_body(cleaned: str) -> str | None:
    """Extract body text from a one-liner function definition.

    Returns text between the function body's ``{`` and matching ``}``
    (after the parameter list), or ``None`` if not detected.
    """
    paren_d = 0
    found_open = False
    param_close = -1
    for ci, ch in enumerate(cleaned):
        if ch == "(":
            if not found_open:
                found_open = True
            paren_d += 1
        elif ch == ")":
            paren_d -= 1
            if paren_d == 0 and found_open:
                param_close = ci
                break

    if param_close < 0:
        return None

    body_open = cleaned.find("{", param_close)
    if body_open < 0:
        return None

    between = cleaned[param_close + 1 : body_open]
    if "= default" in between or "= delete" in between:
        return None

    bd = 0
    body_close = -1
    for ci in range(body_open, len(cleaned)):
        ch = cleaned[ci]
        if ch == "{":
            bd += 1
        elif ch == "}":
            bd -= 1
            if bd == 0:
                body_close = ci
                break

    if body_close < 0:
        return None
    return cleaned[body_open + 1 : body_close]


def _multiline_body_ops(lines: list[str], body_start: int) -> tuple[int, int]:
    """Count top-level operations in a multi-line function body.

    Returns ``(op_count, end_line_exclusive)``.
    """
    depth = 1
    paren = 0
    count = 0
    ctrl_stack: list[bool] = []
    just_closed_ctrl = False
    in_bc = False
    raw_d: str | None = None
    j = body_start

    while j < len(lines):
        bline = lines[j]
        bline, in_bc = advance_past_block_comment(bline, in_bc)
        if in_bc:
            j += 1
            continue
        bline, raw_d = advance_past_multiline_raw_close(bline, raw_d)
        if raw_d is not None:
            j += 1
            continue
        bs = bline.strip()
        if not bs:
            j += 1
            continue
        if bs.startswith("/*") and "*/" not in bs:
            in_bc = True
            j += 1
            continue
        nr = multiline_raw_string_opens(bline)
        if nr is not None:
            raw_d = nr
            j += 1
            continue

        cleaned = strip_strings_and_line_comment(bline)
        for ci, ch in enumerate(cleaned):
            if ch == "(":
                paren += 1
            elif ch == ")":
                paren = max(paren - 1, 0)
            elif ch == "{" and paren == 0:
                if depth == 1:
                    is_ctrl = _preceded_by_ctrl(cleaned, ci)
                    ctrl_stack.append(is_ctrl)
                depth += 1
            elif ch == "}" and paren == 0:
                old = depth
                depth -= 1
                if depth <= 0:
                    return (count, j + 1)
                if old == 2 and depth == 1:
                    was = ctrl_stack.pop() if ctrl_stack else False
                    if was:
                        count += 1
                        just_closed_ctrl = True
            elif ch == ";" and depth == 1 and paren == 0:
                if just_closed_ctrl:
                    just_closed_ctrl = False
                else:
                    count += 1
            elif ch not in " \t\n\r":
                just_closed_ctrl = False
        j += 1

    return (count, j)


def _skip_multiline_body(lines: list[str], start: int) -> int:
    """Advance past a brace-delimited body, returning the next line index."""
    bd = 1
    j = start
    while j < len(lines) and bd > 0:
        for ch in strip_strings_and_line_comment(lines[j]):
            if ch == "{":
                bd += 1
            elif ch == "}":
                bd -= 1
                if bd == 0:
                    break
        j += 1
    return j


# -------------------------------------------------------------------
# Main file checker
# -------------------------------------------------------------------

def check_file(filepath: Path, lines: list[str] | None = None) -> list[str]:
    """Check a single header for multi-operation inline function bodies."""
    violations: list[str] = []

    if not str(filepath).endswith(".h"):
        return violations
    if lines is None:
        try:
            text = filepath.read_text(encoding="utf-8", errors="replace")
            lines = text.splitlines()
        except OSError:
            return violations

    i = 0
    in_block_comment = False
    raw_delim: str | None = None
    prev: list[str] = []

    while i < len(lines):
        line = lines[i]
        line, in_block_comment = advance_past_block_comment(
            line, in_block_comment
        )
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
        nr = multiline_raw_string_opens(line)
        if nr is not None:
            raw_delim = nr
            i += 1
            continue
        if stripped.startswith("//") or stripped.startswith("#"):
            prev.append(line)
            if len(prev) > 10:
                prev = prev[-10:]
            i += 1
            continue

        func_line = i
        is_func = False
        is_oneliner = False

        # Case 1: one-liner — balanced {…} on the same line as (…)
        if "{" in stripped and "}" in stripped and _looks_like_func(stripped):
            cleaned = strip_strings_and_line_comment(line)
            body = _oneliner_body(cleaned)
            if body is not None:
                is_func = True
                is_oneliner = True

        # Case 2: multi-line body — line ends with '{'
        if (
            not is_func
            and stripped.endswith("{")
            and len(stripped) > 1
            and _looks_like_func(stripped)
        ):
            combined = stripped
            for pl in reversed(prev[-5:]):
                ps = pl.strip()
                if not ps or is_full_line_comment(ps):
                    continue
                combined = ps + " " + combined
                if "(" in combined and ")" in combined:
                    break
            if "(" in combined and ")" in combined:
                ft = _first_token(combined)
                if ft not in _CONTROL_FLOW and ft not in _TYPE_DEF_KW:
                    is_func = True

        # Case 3: standalone '{' preceded by function signature
        if not is_func and stripped == "{" and prev:
            combined = ""
            for pl in reversed(prev[-5:]):
                ps = pl.strip()
                if not ps:
                    break
                if ps.startswith("//"):
                    continue
                combined = ps + " " + combined
                if "(" in combined and ")" in combined:
                    ft = _first_token(combined)
                    if ft and ft not in _CONTROL_FLOW and ft not in _TYPE_DEF_KW:
                        is_func = True
                    break

        if not is_func:
            prev.append(line)
            if len(prev) > 10:
                prev = prev[-10:]
            i += 1
            continue

        # --- Exemptions ---
        sig = _build_sig_text(prev, line)
        exempt = _is_template_context(sig, prev) or _sig_is_exempt(sig)

        if exempt:
            if is_oneliner:
                i += 1
            else:
                i = _skip_multiline_body(lines, i + 1)
            prev = []
            continue

        # --- Count operations ---
        if is_oneliner:
            cleaned = strip_strings_and_line_comment(line)
            body_text = _oneliner_body(cleaned) or ""
            ops = _count_body_ops(body_text)
            next_i = i + 1
        else:
            ops, next_i = _multiline_body_ops(lines, i + 1)

        if ops > MAX_INLINE_OPS:
            sig_lines = lines[max(func_line - 3, 0) : func_line + 1]
            name = extract_function_name_from_sig_lines(sig_lines)
            violations.append(
                f"{filepath}:{func_line + 1}: [inline-body] "
                f"function '{name}' has {ops} operations inline "
                f"(max {MAX_INLINE_OPS} in header; move body to .cpp)"
            )

        i = next_i
        prev = []

    return violations


def main() -> int:
    args = parse_common_args(
        "Check header inline function bodies (max 1 operation)"
    )
    files = discover_files(
        paths=args.paths or None,
        src_only=args.src_only,
        tests_only=args.tests_only,
    )
    all_headers = files.all_headers
    if not all_headers:
        print("No header files to check.")
        return 0

    total = 0
    for f in all_headers:
        for v in check_file(f):
            print(v)
            total += 1

    if total > 0:
        print(f"\n{total} inline-body violation(s) found.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
