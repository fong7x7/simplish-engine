#!/usr/bin/env python3
"""Check that functions have at most 4 parameters (including implicit 'this').

Output format: file:line: [param-count] message
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

MAX_PARAMS = 4

# Keywords that introduce blocks but are not function definitions.
NON_FUNC_KW = {
    "if", "else", "for", "while", "do", "switch", "case", "catch", "try",
    "struct", "class", "enum", "namespace", "union", "typedef", "using",
    "template",
    "return", "throw", "co_return", "co_await", "co_yield",
}

# Macros that look like functions but aren't (Catch2, assert, etc.)
MACRO_PREFIXES = (
    "TEST_CASE", "SECTION", "REQUIRE", "CHECK", "BENCHMARK",
    "ENGINE_ASSERT", "ENGINE_EVENT_TRAIT", "ENG_", "CATCH_",
    "INTERNAL_CATCH", "TEMPLATE_TEST_CASE",
    "GENERATE", "STATIC_REQUIRE", "INFO", "WARN", "FAIL",
    "SCENARIO", "GIVEN", "WHEN", "THEN", "AND_GIVEN", "AND_WHEN", "AND_THEN",
)

# C++ fundamental types that can appear before '(' but are never function names.
_FUNDAMENTAL_TYPES = frozenset({
    "bool", "int", "float", "double", "char", "void", "auto",
    "long", "short", "unsigned", "signed", "wchar_t",
    "char8_t", "char16_t", "char32_t",
    "int8_t", "int16_t", "int32_t", "int64_t",
    "uint8_t", "uint16_t", "uint32_t", "uint64_t",
    "size_t", "ptrdiff_t", "nullptr_t",
})

_NAME_SKIP = NON_FUNC_KW | _FUNDAMENTAL_TYPES | frozenset({
    "sizeof", "decltype", "static_assert", "alignof", "typeid",
    "noexcept", "offsetof", "alignas", "concept", "requires",
    "const_cast", "static_cast", "dynamic_cast", "reinterpret_cast",
})


def _is_macro_call(name: str) -> bool:
    """Check if a function name is actually a macro call."""
    if name.isupper() or name.startswith("ENGINE_"):
        return True
    for prefix in MACRO_PREFIXES:
        if name.startswith(prefix):
            return True
    return False


# Regex for detecting '=' that is assignment, not comparison (==, !=, <=, >=).
_ASSIGN_RE = re.compile(r"(?<![!=<>])=(?!=)")


def _extract_name(combined: str) -> str:
    """Extract the function name from a combined signature line (no context)."""
    for m in re.finditer(r"(\w+)\s*\(", combined):
        cand = m.group(1)
        if cand in _NAME_SKIP:
            continue
        if _is_macro_call(cand):
            continue
        return cand
    return "<unknown>"


def _count_params(param_str: str) -> int:
    """Count the number of parameters in a parameter list string.

    Handles nested templates, parentheses, and brace-initializer lists.
    """
    param_str = param_str.strip()
    if not param_str or param_str == "void":
        return 0

    depth_angle = 0
    depth_paren = 0
    depth_brace = 0
    count = 1

    for ch in param_str:
        if ch == "<" and depth_paren == 0 and depth_brace == 0:
            depth_angle += 1
        elif ch == ">" and depth_angle > 0 and depth_paren == 0 and depth_brace == 0:
            depth_angle -= 1
        elif ch == "(":
            depth_paren += 1
        elif ch == ")":
            depth_paren -= 1
        elif ch == "{":
            depth_brace += 1
        elif ch == "}":
            depth_brace -= 1
        elif (
            ch == ","
            and depth_angle == 0
            and depth_paren == 0
            and depth_brace == 0
        ):
            count += 1

    return count


def check_file(filepath: Path, lines: list[str] | None = None) -> list[str]:
    """Check a single file for parameter count violations."""
    violations: list[str] = []
    if lines is None:
        try:
            lines = filepath.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            return violations

    # Track whether we're inside a class/struct for implicit 'this'.
    # Simplified: track brace depth and note when we enter a class/struct.
    class_depth: list[int] = []  # brace depth at which each class/struct starts
    brace_depth = 0
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

        # Skip comment and preprocessor lines
        if stripped.startswith("//") or stripped.startswith("#"):
            i += 1
            continue

        # Track class/struct scope entries
        class_match = re.match(
            r"\s*(?:template\s*<[^>]*>\s*)?(struct|class)\s+(\w+)", line
        )
        if class_match and "{" in line:
            class_depth.append(brace_depth)

        # Track brace depth
        for ch in stripped:
            if ch == "{":
                brace_depth += 1
            elif ch == "}":
                brace_depth -= 1
                # Pop class scope if we've closed it
                if class_depth and brace_depth <= class_depth[-1]:
                    class_depth.pop()

        # Look for function declarations/definitions with '(' on this line
        if "(" not in stripped:
            i += 1
            continue

        # Skip lines that are clearly not function definitions
        first_token = stripped.split()[0] if stripped else ""
        first_token = first_token.split("<")[0].rstrip("(")
        if first_token in NON_FUNC_KW:
            i += 1
            continue

        # Try to find a complete parameter list starting from this line.
        # Accumulate lines until we find the matching ')'.
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

        # Extract function name from the combined signature (not context).
        name = _extract_name(combined)
        if name == "<unknown>":
            i += 1
            continue

        # Skip macros
        if _is_macro_call(name):
            i += 1
            continue

        # Skip lambda expressions
        if "= [" in combined or "=[" in combined or combined.strip().startswith("["):
            i += 1
            continue

        # Locate name( in combined for before-name analysis.
        name_match = re.search(rf"\b{re.escape(name)}\s*\(", combined)
        if not name_match:
            i += 1
            continue

        before_name = combined[: name_match.start()]
        before_stripped = before_name.rstrip()

        # Method call: name preceded by '.' or '->'
        if before_stripped.endswith(".") or before_stripped.endswith("->"):
            i += 1
            continue

        # Assignment context: '=' (not ==, !=, <=, >=) before the name.
        # Only check outside template angle brackets so that SFINAE
        # defaults like ``enable_if_t<cond>* = nullptr`` are ignored.
        first_angle = before_name.find("<")
        last_close = before_name.rfind(">")
        before_tmpl = before_name[:first_angle] if first_angle >= 0 else before_name
        after_tmpl = before_name[last_close + 1 :] if last_close >= 0 else ""
        if _ASSIGN_RE.search(before_tmpl) or _ASSIGN_RE.search(after_tmpl):
            i += 1
            continue

        # Namespace-qualified call: "ns::name(" with no return type.
        # Out-of-line definitions have a return type or are constructors
        # (qualifier matches name).
        if before_stripped.endswith("::"):
            pre_colons = before_stripped[:-2].rstrip()
            last_qualifier = pre_colons.split("::")[-1].strip()
            is_constructor = last_qualifier == name
            has_return_type = " " in pre_colons or "\t" in pre_colons
            if not is_constructor and not has_return_type:
                i += 1
                continue

        # Inside another expression's argument list (preceded by ',' or '(').
        if before_stripped.endswith(",") or before_stripped.endswith("("):
            i += 1
            continue

        # Find the '(' for the parameter list and its matching ')'.
        open_idx = name_match.end() - 1
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

        # Determine declaration vs definition from tail after matching ')'.
        tail = combined[close_idx + 1 :].strip()
        is_declaration = ";" in tail and "{" not in tail
        is_definition = "{" in tail or (
            j < len(lines)
            and lines[j].strip().startswith("{")
            if j < len(lines)
            else False
        )

        # Not a declaration or definition — likely a function call.
        if not is_declaration and not is_definition:
            if not before_stripped:
                i += 1
                continue

        # Bare call: no return type before name and not in a class body.
        if not before_stripped and len(class_depth) == 0:
            i += 1
            continue

        # Extract and count parameters.
        param_str = combined[open_idx + 1 : close_idx].strip()
        param_count = _count_params(param_str)

        # Add implicit 'this' for non-static, non-friend member functions.
        is_member = len(class_depth) > 0
        is_static = "static " in combined[: open_idx]
        is_friend = "friend " in combined[: open_idx]
        has_this = is_member and not is_static and not is_friend

        total = param_count + (1 if has_this else 0)

        if total > MAX_PARAMS:
            this_note = " including 'this'" if has_this else ""
            violations.append(
                f"{filepath}:{i + 1}: [param-count] "
                f"function '{name}' has {total} parameters{this_note} "
                f"(limit: {MAX_PARAMS})"
            )

        i += 1
        continue

    return violations


def main() -> int:
    args = parse_common_args("Check function parameter count limit (4 max)")
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
        print(f"\n{total_violations} param-count violation(s) found.")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
