#!/usr/bin/env python3
"""Check that every struct/class field has a /// doc comment.

Per CLAUDE.md: "Every field of a struct or class must have a 1-line ///
comment describing its purpose. No undocumented members."

Output format: file:line: [member-comment] message
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

# Access specifiers
ACCESS_RE = re.compile(r"^\s*(public|private|protected)\s*:")

# Lines that are clearly not member variables
SKIP_PATTERNS = [
    re.compile(r"^\s*//"),           # comments
    re.compile(r"^\s*/\*"),          # block comment start
    re.compile(r"^\s*\*"),           # block comment continuation
    re.compile(r"^\s*$"),            # blank
    re.compile(r"^\s*#"),            # preprocessor
    re.compile(r"^\s*using\s"),      # type aliases
    re.compile(r"^\s*typedef\s"),    # typedefs
    re.compile(r"^\s*friend\s"),     # friend declarations
    re.compile(r"^\s*static_assert"),  # static asserts
    re.compile(r"^\s*enum\s"),       # nested enums
    re.compile(r"^\s*struct\s"),     # nested structs
    re.compile(r"^\s*class\s"),      # nested classes
    re.compile(r"^\s*union\s"),      # nested unions
    re.compile(r"^\s*template\s*<"), # template declarations
    re.compile(r"^\s*\{"),           # opening brace
    re.compile(r"^\s*\}"),           # closing brace
]

# Member variable heuristic: a line ending with ';' that contains a type + name,
# is not a function (no '(' unless it's a function pointer), and is not a
# using/typedef/static_assert.
# Allow optional whitespace between trailing qualifiers (e.g. "const = 0").
FUNC_DECL_RE = re.compile(
    r"\(.*\)\s*(?:\s*(?:const|override|final|noexcept|=\s*0|=\s*default|=\s*delete))*\s*;"
)


def _is_member_variable(line: str) -> bool:
    """Heuristic: is this line a member variable declaration?"""
    stripped = line.strip()

    # Must end with ';'
    if not stripped.endswith(";"):
        return False

    # Skip lines matching non-variable patterns
    for pat in SKIP_PATTERNS:
        if pat.match(line):
            return False

    # Virtual methods and pure-virtual / continuation lines are not data members.
    if re.match(r"^\s*virtual\s+", line):
        return False
    if re.search(r"\)\s*const\s*=", stripped):
        return False
    if re.search(r"\)\s*=\s*0\s*;", stripped):
        return False
    if re.search(r"\)\s*=\s*default\s*;", stripped):
        return False
    if re.search(r"\)\s*=\s*delete\s*;", stripped):
        return False
    if re.search(r"\)\s*override\s*;", stripped):
        return False
    if re.search(r"\)\s*final\s*;", stripped):
        return False
    if re.search(r"\)\s*noexcept\s*;", stripped):
        return False
    # Declarator continuation: closing paren on this line but no `(` (parameters
    # opened on a previous line for methods, function pointers, or macros).
    if "(" not in stripped and ")" in stripped and stripped.endswith(";"):
        return False

    # Trailing return type continuation (`auto foo() -> T;` split across lines).
    if re.match(r"^\s*->\s+", stripped):
        return False

    # Skip function declarations (contain parentheses for params)
    if FUNC_DECL_RE.search(stripped):
        return False

    # Skip lines that are just a closing brace with semicolon
    if stripped == "};":
        return False

    # Skip 'static constexpr' constants (these are constants, not fields)
    if "static constexpr" in stripped or "static const" in stripped:
        return False

    # Must have at least a type and a name (2+ tokens before the semicolon)
    no_semi = stripped.rstrip(";").strip()
    # Handle default initialization: int x = 0; or int x{0};
    no_init = re.sub(r"\s*[={].*$", "", no_semi).strip()
    tokens = no_init.split()
    if len(tokens) < 2:
        return False

    return True


def _prev_line_has_doc_comment(lines: list[str], idx: int) -> bool:
    """Check if the line(s) immediately before idx contain a /// comment."""
    for j in range(idx - 1, max(idx - 5, -1), -1):
        stripped = lines[j].strip()
        if not stripped:
            continue  # skip blank lines between comment and field
        if stripped.startswith("///"):
            return True
        # Also accept /** ... */ style doc comments
        if stripped.startswith("/**") or stripped.endswith("*/"):
            return True
        return False
    return False


def check_file(filepath: Path, lines: list[str] | None = None) -> list[str]:
    """Check a single file for undocumented member variables."""
    violations: list[str] = []
    if lines is None:
        try:
            lines = filepath.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            return violations

    brace_depth = 0
    in_type_body = False
    type_brace_depths: list[int] = []  # stack of brace depths where types start
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

        # Skip preprocessor and pure comment lines for structure tracking
        if stripped.startswith("#"):
            i += 1
            continue
        if stripped.startswith("//"):
            i += 1
            continue

        # Detect struct/class definition opening
        type_match = re.match(
            r"\s*(?:template\s*<[^>]*>\s*)?(struct|class)\s+\w+", line
        )
        if type_match and ("{" in stripped or (
            i + 1 < len(lines) and lines[i + 1].strip().startswith("{")
        )):
            # Will enter type body when we see the '{'
            if "{" in stripped:
                type_brace_depths.append(brace_depth)

        # Track brace depth
        for ch in stripped:
            if ch == "{":
                brace_depth += 1
            elif ch == "}":
                brace_depth -= 1
                if type_brace_depths and brace_depth <= type_brace_depths[-1]:
                    type_brace_depths.pop()

        # Standalone '{' after a type definition
        if stripped == "{" and i > 0:
            prev = lines[i - 1].strip() if i > 0 else ""
            if re.match(r"(?:template\s*<[^>]*>\s*)?(struct|class)\s+\w+", prev):
                type_brace_depths.append(brace_depth - 1)

        in_type_body = len(type_brace_depths) > 0

        # Check member variables inside type bodies
        if in_type_body and brace_depth > 0:
            # Only check at the immediate level of the innermost type
            if type_brace_depths and brace_depth == type_brace_depths[-1] + 1:
                if _is_member_variable(line):
                    if not _prev_line_has_doc_comment(lines, i):
                        # Extract field name
                        no_semi = stripped.rstrip(";").strip()
                        no_init = re.sub(r"\s*[={].*$", "", no_semi).strip()
                        tokens = no_init.split()
                        field_name = tokens[-1] if tokens else "<unknown>"
                        # Clean up pointer/reference decorators
                        field_name = field_name.lstrip("*&")

                        violations.append(
                            f"{filepath}:{i + 1}: [member-comment] "
                            f"field '{field_name}' has no /// doc comment"
                        )

        i += 1

    return violations


def main() -> int:
    args = parse_common_args("Check that struct/class fields have /// doc comments")
    files = discover_files(
        paths=args.paths or None,
        src_only=args.src_only,
        tests_only=args.tests_only,
    )

    # Check headers primarily (that's where fields are declared)
    check_files = files.all_headers
    if not check_files:
        print("No header files to check.")
        return 0

    total_violations = 0
    for f in check_files:
        violations = check_file(f)
        for v in violations:
            print(v)
            total_violations += 1

    if total_violations > 0:
        print(f"\n{total_violations} member-comment violation(s) found.")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
