#!/usr/bin/env python3
"""Turn CTest console output into a markdown failure checklist.

Usage:
    ctest_checklist.py OUTPUT_FILE CHECKLIST_PATH BUILD_TYPE [FILTER]

Prints the number of failures on stdout.

CTest reports a failure as::

    1/34 Test #96: <name> ...***Failed    0.01 sec

Catch2 names its tests by their ``TEST_CASE`` title, so ``<name>`` is a prose
sentence containing spaces, commas, and regex metacharacters. That is why this
lives in Python rather than a sed pipeline: the previous shell version captured
``[^ ]+`` and truncated every name at its first space, which also broke the
lookup that slices out each failure's output.
"""

from __future__ import annotations

import re
import sys
from datetime import datetime, timezone

# "  1/34 Test #96: some name ...***Failed    0.01 sec"
FAILED_RE = re.compile(
    r"^\s*\d+/\d+\s+Test\s+#(\d+):\s+(.*?)\s*\.*\*\*\*(Failed|Exception|Timeout)"
)
# "    Start 96: some name"
START_RE = re.compile(r"^\s*Start\s+(\d+):\s")
# A result line, which terminates a failure's output block.
RESULT_RE = re.compile(r"^\s*\d+/\d+\s+Test\s+#\d+:")
SUMMARY_RE = re.compile(r"^\d+% tests passed")

MAX_OUTPUT_LINES = 120


def find_failures(lines: list[str]) -> list[tuple[int, str, str]]:
    """Return (line_index, test_name, status) for every reported failure."""
    failures = []
    for index, line in enumerate(lines):
        match = FAILED_RE.match(line)
        if match:
            failures.append((index, match.group(2).strip(), match.group(3)))
    return failures


def output_for(lines: list[str], failure_index: int) -> list[str]:
    """The captured output that follows a failure's result line.

    Under ``ctest -j`` every ``Start N:`` marker is emitted up front, in a
    batch, so a test's output does not follow its own marker. With
    ``--output-on-failure`` the output is printed directly after the
    ``***Failed`` line instead, and ends at the next ``Start`` or result line.
    """
    block: list[str] = []
    for line in lines[failure_index + 1:]:
        if START_RE.match(line) or RESULT_RE.match(line):
            break
        block.append(line)
    while block and not block[0].strip():
        block.pop(0)
    while block and not block[-1].strip():
        block.pop()
    return block[:MAX_OUTPUT_LINES]


def find_summary(lines: list[str]) -> str:
    for line in lines:
        if SUMMARY_RE.match(line):
            return line.strip()
    return ""


def render(
    lines: list[str],
    failures: list[tuple[int, str, str]],
    build_type: str,
    test_filter: str,
) -> str:
    stamp = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    out = ["# Test Failure Checklist", ""]
    out.append(f"**Generated:** {stamp}")
    out.append(f"**Build type:** {build_type}")
    if test_filter:
        out.append(f"**Filter:** `{test_filter}`")
    summary = find_summary(lines)
    if summary:
        out.append(f"**Result:** {summary}")
    out.append("")

    if not failures:
        out.append("All tests passed. No failures to report.")
        return "\n".join(out) + "\n"

    out += ["## Summary", "", "| # | Test | Status |", "|---|------|--------|"]
    for index, (_line, name, status) in enumerate(failures, start=1):
        out.append(f"| {index} | `{name}` | {status} |")

    out += ["", "## Checklist", ""]
    for _line, name, _status in failures:
        out.append(f"- [ ] `{name}`")

    out += ["", "## Failure Output", ""]
    for line_index, name, _status in failures:
        block = output_for(lines, line_index) or ["(no output captured)"]
        out += [f"### `{name}`", "", "<details>", "<summary>Output</summary>", ""]
        out.append("```")
        out += block
        out.append("```")
        out += ["", "</details>", ""]

    return "\n".join(out) + "\n"


def main(argv: list[str]) -> int:
    if len(argv) < 4:
        print(__doc__, file=sys.stderr)
        return 2

    output_file, checklist_path, build_type = argv[1:4]
    test_filter = argv[4] if len(argv) > 4 else ""

    try:
        with open(output_file, "r", errors="replace") as handle:
            lines = handle.read().splitlines()
    except OSError as error:
        print(f"Cannot read {output_file}: {error}", file=sys.stderr)
        return 2

    failures = find_failures(lines)
    markdown = render(lines, failures, build_type, test_filter)

    try:
        with open(checklist_path, "w") as handle:
            handle.write(markdown)
    except OSError as error:
        print(f"Cannot write {checklist_path}: {error}", file=sys.stderr)
        return 2

    print(len(failures))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
