"""Shared helpers for C++ heuristic scanners (check-*.py scripts)."""

from __future__ import annotations

import re
from typing import Optional

__all__ = [
    "is_full_line_comment",
    "advance_past_block_comment",
    "advance_past_multiline_raw_close",
    "multiline_raw_string_opens",
    "extract_function_name_from_sig_lines",
    "strip_strings_and_line_comment",
]


def advance_past_block_comment(line: str, in_block_comment: bool) -> tuple[str, bool]:
    """Return (line to scan, still_inside_block_comment).

    When inside a block comment, lines without ``*/`` are fully skipped (caller
    should not scan). When ``*/`` appears, scanning resumes with the suffix
    after the first ``*/`` so code on the same line is not missed.
    """
    if not in_block_comment:
        return (line, False)
    if "*/" not in line:
        return ("", True)
    return (line[line.find("*/") + 2 :], False)


def advance_past_multiline_raw_close(
    line: str, raw_delim: Optional[str]
) -> tuple[str, Optional[str]]:
    """When inside ``R\"delim(`` … ``)delim\"``, advance past the close on this line.

    If ``raw_delim`` is ``None``, returns ``(line, None)``. If the closing
    sequence is not on this line, returns ``(\"\", raw_delim)`` so the caller
    skips the line. Otherwise returns ``(suffix_after_close, None)`` so code
    after the raw literal on the same line is still scanned.
    """
    if raw_delim is None:
        return (line, None)
    closing = f'){raw_delim}"'
    pos = line.find(closing)
    if pos == -1:
        return ("", raw_delim)
    return (line[pos + len(closing) :], None)


def is_full_line_comment(line: str) -> bool:
    """True if the line is only a line or block comment (or blank)."""
    stripped = line.strip()
    if not stripped:
        return True
    if stripped.startswith("//"):
        return True
    if stripped.startswith("/*") and stripped.endswith("*/"):
        return True
    return False


def _consume_raw_string_same_line(line: str, r_index: int) -> int:
    """Return index after a raw literal R\"delim(...)delim\" starting at r_index."""
    lparen = line.find("(", r_index + 2)
    if lparen == -1:
        return min(r_index + 2, len(line))
    delim = line[r_index + 2 : lparen]
    closing = f"){delim}\""
    close_idx = line.find(closing, lparen + 1)
    if close_idx == -1:
        return len(line)
    return close_idx + len(closing)


def _starts_raw_literal(line: str, i: int) -> bool:
    if i + 1 >= len(line) or line[i] != "R" or line[i + 1] != '"':
        return False
    if i == 0:
        return True
    prev = line[i - 1]
    return not (prev.isalnum() or prev == "_")


def _raw_literal_starts_at(line: str, i: int) -> bool:
    """True if a raw string literal starts at ``i`` (``R\"`` or ``u8R\"``, ``LR\"``, …)."""
    if _starts_raw_literal(line, i):
        return True
    if line.startswith("u8R\"", i):
        return True
    if (
        line.startswith("LR\"", i)
        or line.startswith("UR\"", i)
        or line.startswith("uR\"", i)
    ):
        return True
    return False


def _r_char_index_at_raw_start(line: str, i: int) -> int:
    """Index of ``R`` for a raw literal known to start at ``i``."""
    if line.startswith("u8R\"", i):
        return i + 2
    if (
        line.startswith("LR\"", i)
        or line.startswith("UR\"", i)
        or line.startswith("uR\"", i)
    ):
        return i + 1
    return i


def _raw_literal_lexeme(line: str, i: int) -> Optional[tuple[int, int]]:
    """If a raw literal begins at ``i``, return ``(start_i, end_exclusive)``."""
    if not _raw_literal_starts_at(line, i):
        return None
    r_pos = _r_char_index_at_raw_start(line, i)
    end = _consume_raw_string_same_line(line, r_pos)
    if end <= i:
        return None
    return (i, end)


def multiline_raw_string_opens(line: str) -> Optional[str]:
    """If line opens R\"delim( without )delim\" on the same line, return delim."""
    i = 0
    n = len(line)
    while i < n:
        if not _raw_literal_starts_at(line, i):
            i += 1
            continue
        r = _r_char_index_at_raw_start(line, i)
        lparen = line.find("(", r + 2)
        if lparen == -1:
            return None
        delim = line[r + 2 : lparen]
        closing = f"){delim}\""
        close_idx = line.find(closing, lparen + 1)
        if close_idx == -1:
            return delim
        nxt = close_idx + len(closing)
        i = nxt if nxt > i else i + 1
    return None


def extract_function_name_from_sig_lines(sig_lines: list[str]) -> str:
    """Best-effort name from signature lines (ignores doc/NOLINT full-line comments)."""
    code_only = [ln for ln in sig_lines if not is_full_line_comment(ln)]
    sig = " ".join(ln.strip() for ln in code_only)
    matches = list(re.finditer(r"(\w+)\s*\(", sig))
    skip = frozenset(
        {
            "if",
            "for",
            "while",
            "switch",
            "case",
            "catch",
            "throw",
            "return",
            "sizeof",
            "decltype",
            "static_assert",
            "co_return",
            "co_await",
            "co_yield",
        }
    )
    for m in matches:
        name = m.group(1)
        if name in skip or name.startswith("NOLINT"):
            continue
        return name
    return "<unknown>"


def strip_strings_and_line_comment(line: str) -> str:
    """Blank out // comments and string/char literals so keyword searches skip strings."""
    i = 0
    n = len(line)
    parts: list[str] = []
    while i < n:
        if i + 1 < n and line[i : i + 2] == "//":
            parts.append(" " * (n - i))
            break
        if i + 1 < n and line[i : i + 2] == "/*":
            end = line.find("*/", i + 2)
            if end == -1:
                parts.append(" " * (n - i))
                break
            parts.append(" " * (end + 2 - i))
            i = end + 2
            continue
        raw_lex = _raw_literal_lexeme(line, i)
        if raw_lex is not None:
            start_i, end = raw_lex
            parts.append(" " * (end - start_i))
            i = end
            continue
        c = line[i]
        if i + 3 <= n and line.startswith("u8\"", i):
            parts.append("  ")
            i += 2
            c = line[i]
        elif i + 1 < n and line[i] in "uUL" and line[i + 1] == '"':
            parts.append(" ")
            i += 1
            c = line[i]
        if c == '"':
            parts.append(" ")
            i += 1
            while i < n:
                if line[i] == "\\" and i + 1 < n:
                    parts.append("  ")
                    i += 2
                    continue
                if line[i] == '"':
                    parts.append(" ")
                    i += 1
                    break
                parts.append(" ")
                i += 1
            continue
        if c == "'":
            # C++14 digit separator: ' between two digit / hex chars (not char lit).
            def _hex_or_digit(ch: str) -> bool:
                return ch.isdigit() or ch in "abcdefABCDEF"

            if (
                i > 0
                and i + 1 < n
                and _hex_or_digit(line[i - 1])
                and _hex_or_digit(line[i + 1])
            ):
                parts.append(c)
                i += 1
                continue
            parts.append(" ")
            i += 1
            while i < n:
                if line[i] == "\\" and i + 1 < n:
                    parts.append("  ")
                    i += 2
                    continue
                if line[i] == "'":
                    parts.append(" ")
                    i += 1
                    break
                parts.append(" ")
                i += 1
            continue
        parts.append(c)
        i += 1
    return "".join(parts)
