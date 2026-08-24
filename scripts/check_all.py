#!/usr/bin/env python3
"""Unified Simplish invariant checker.

Discovers files once, reads each file once, runs all selected checks in a
single process.  Drop-in replacement for the subprocess-per-check
orchestration previously done by ``check-invariants.sh``.

Usage: python3 scripts/check_all.py [OPTIONS] [PATH...]

Options:
  --check NAME        Run only the named check (repeat for several)
  --list              List all available checks
  --src-only          Check only production code (not with --tests-only)
  --tests-only        Check only test code (not with --src-only)
  --checklist [FILE]  Output markdown checklist (default: invariant-checklist.md)
  -q, --quiet         Only print violations (no progress or summary)
"""

from __future__ import annotations

import argparse
import importlib.util
import sys
from collections import defaultdict
from dataclasses import dataclass
from datetime import date
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent

sys.path.insert(0, str(SCRIPT_DIR / "lib"))

from find_sources import discover_files


# ---------------------------------------------------------------------------
# Check registry
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class CheckDef:
    """Metadata for one invariant check."""

    name: str
    tag: str
    description: str
    script: str
    file_filter: str  # "all" | "headers"


CHECK_DEFS: list[CheckDef] = [
    CheckDef(
        "function-size",
        "[function-size]",
        "16-line function body limit (50 for named algorithms)",
        "check-function-size",
        "all",
    ),
    CheckDef(
        "param-count",
        "[param-count]",
        "4-parameter maximum (including this)",
        "check-param-count",
        "all",
    ),
    CheckDef(
        "one-type-per-file",
        "[one-type-per-file]",
        "One struct/class per header file",
        "check-one-type-per-file",
        "headers",
    ),
    CheckDef(
        "member-comments",
        "[member-comment]",
        "/// doc comment on every struct/class field",
        "check-member-comments",
        "headers",
    ),
    CheckDef(
        "bool-params",
        "[bool-param]",
        "No bool function parameters (use enum class)",
        "check-bool-params",
        "all",
    ),
    CheckDef(
        "bare-new-delete",
        "[bare-new-delete]",
        "No bare new/delete (use smart pointers)",
        "check-bare-new-delete",
        "all",
    ),
    CheckDef(
        "no-exceptions",
        "[no-exceptions]",
        "No throw/try/catch (use optional/expected)",
        "check-no-exceptions",
        "all",
    ),
    CheckDef(
        "commented-code",
        "[commented-code]",
        "No commented-out code blocks",
        "check-no-commented-code",
        "all",
    ),
    CheckDef(
        "inline-body",
        "[inline-body]",
        "Header inline function bodies limited to one operation",
        "check-inline-body",
        "headers",
    ),
    CheckDef(
        "no-forward-decl",
        "[no-forward-decl]",
        "No forward declarations (include the correct header)",
        "check-no-forward-decl",
        "headers",
    ),
    CheckDef(
        "trailing-return",
        "[trailing-return]",
        "No trailing return types (use standard return type)",
        "check-trailing-return",
        "all",
    ),
]


# ---------------------------------------------------------------------------
# Module loader
# ---------------------------------------------------------------------------


def _load_module(script_name: str) -> Any:
    """Import ``scripts/<script_name>.py`` as a Python module."""
    path = SCRIPT_DIR / f"{script_name}.py"
    mod_name = script_name.replace("-", "_")
    spec = importlib.util.spec_from_file_location(mod_name, path)
    if spec is None or spec.loader is None:
        raise ImportError(f"Cannot load checker script: {path}")
    mod = importlib.util.module_from_spec(spec)
    sys.modules[mod_name] = mod
    spec.loader.exec_module(mod)
    return mod


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def _parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Run all CLAUDE.md coding-invariant checks.",
    )
    p.add_argument(
        "--check",
        action="append",
        dest="checks",
        metavar="NAME",
        help="Run only the named check (repeat for several)",
    )
    p.add_argument("--list", action="store_true", help="List available checks")
    scope = p.add_mutually_exclusive_group()
    scope.add_argument("--src-only", action="store_true")
    scope.add_argument("--tests-only", action="store_true")
    p.add_argument(
        "--checklist",
        nargs="?",
        const="invariant-checklist.md",
        default=None,
        metavar="FILE",
        help="Output markdown checklist (default: invariant-checklist.md)",
    )
    p.add_argument(
        "-q",
        "--quiet",
        action="store_true",
        help="Only print violations (no progress or summary)",
    )
    p.add_argument(
        "--progress",
        action="store_true",
        help="Show file progress even when --quiet is set",
    )
    p.add_argument("paths", nargs="*", help="Files or directories to check")
    return p.parse_args()


# ---------------------------------------------------------------------------
# Check execution
# ---------------------------------------------------------------------------


def _select_checks(requested: list[str] | None) -> list[CheckDef]:
    """Return the ordered list of checks to run."""
    if not requested:
        return list(CHECK_DEFS)
    known = {c.name for c in CHECK_DEFS}
    unknown = set(requested) - known
    if unknown:
        for n in sorted(unknown):
            print(
                f"Error: unknown check '{n}'. Use --list for names.",
                file=sys.stderr,
            )
        sys.exit(1)
    seen: set[str] = set()
    active: list[CheckDef] = []
    for c in CHECK_DEFS:
        if c.name in requested and c.name not in seen:
            active.append(c)
            seen.add(c.name)
    return active


def _run_checks(
    active: list[CheckDef],
    all_files: list[Path],
    header_set: frozenset[str],
    *,
    progress: bool = False,
) -> dict[str, list[str]]:
    """Run *active* checks over *all_files*, reading each file once."""
    modules: dict[str, Any] = {}
    for check in active:
        modules[check.name] = _load_module(check.script)

    results: dict[str, list[str]] = {c.name: [] for c in active}
    total = len(all_files)

    for i, filepath in enumerate(all_files, 1):
        if progress:
            pct = i * 100 // total
            print(
                f"\r  check-invariants: {pct}% ({i}/{total} files)",
                end="",
                file=sys.stderr,
                flush=True,
            )

        try:
            lines = filepath.read_text(
                encoding="utf-8",
                errors="replace",
            ).splitlines()
        except OSError:
            continue

        is_header = str(filepath) in header_set
        for check in active:
            if check.file_filter == "headers" and not is_header:
                continue
            try:
                violations = modules[check.name].check_file(filepath, lines=lines)
                results[check.name].extend(violations)
            except Exception as exc:
                print(
                    f"  Warning: {check.name} failed on {filepath}: {exc}",
                    file=sys.stderr,
                )

    if progress:
        print(
            f"\r  check-invariants: 100% ({total}/{total} files)",
            file=sys.stderr,
            flush=True,
        )

    return results


# ---------------------------------------------------------------------------
# Console output
# ---------------------------------------------------------------------------


def _print_console(
    active: list[CheckDef],
    results: dict[str, list[str]],
    total_violations: int,
    failed_checks: int,
    *,
    quiet: bool,
    checklist_mode: bool,
) -> None:
    show_progress = not quiet and not checklist_mode
    show_violations = not checklist_mode

    if show_progress:
        print("\n  Simplish Invariant Checks")
        print("  =========================\n")

    for check in active:
        v = results[check.name]
        if show_progress:
            status = "PASS" if not v else f"FAIL  ({len(v)} violation(s))"
            print(f"  {check.name:<22s} {status}")
        if show_violations and v:
            for line in v:
                print(line)

    if show_progress:
        print()
        if total_violations == 0:
            print("  Result: All checks passed.")
        else:
            print(
                f"  Result: {total_violations} violation(s) "
                f"in {failed_checks} check(s).",
            )
        print()


# ---------------------------------------------------------------------------
# Checklist generation
# ---------------------------------------------------------------------------


def _relativize(text: str, prefix: str) -> str:
    return text.replace(prefix + "/", "")


def _violation_filepath(violation: str) -> str:
    """Extract the file path from a violation line."""
    return violation.split(":")[0]


def _build_scope_label(args: argparse.Namespace) -> str:
    if args.paths:
        return " ".join(args.paths)
    if args.src_only:
        return "production code only"
    if args.tests_only:
        return "test code only"
    return "all files"


def _generate_checklist(
    checklist_file: str,
    active: list[CheckDef],
    results: dict[str, list[str]],
    total_violations: int,
    failed_checks: int,
    scope_label: str,
    project_root: str,
) -> None:
    """Write a markdown checklist to *checklist_file*."""
    rel: dict[str, list[str]] = {
        c.name: [_relativize(v, project_root) for v in results[c.name]]
        for c in active
    }

    all_violation_files: set[str] = set()
    file_counts: dict[str, int] = {}
    for check in active:
        files_for_check: set[str] = set()
        for v in rel[check.name]:
            fp = _violation_filepath(v)
            files_for_check.add(fp)
            all_violation_files.add(fp)
        file_counts[check.name] = len(files_for_check)

    out: list[str] = []

    out.append("# Invariant Findings Checklist")
    out.append("")
    out.append(f"**Generated:** {date.today().isoformat()}")
    out.append(f"**Scope:** {scope_label}")
    out.append(
        f"**Total findings:** {total_violations} "
        f"across {len(all_violation_files)} files",
    )
    out.append("")

    dir_counts: dict[str, int] = defaultdict(int)
    for check in active:
        for v in rel[check.name]:
            fp = _violation_filepath(v)
            top = fp.split("/")[0] if "/" in fp else fp
            dir_counts[top] += 1
    if dir_counts:
        parts = sorted(dir_counts.items(), key=lambda kv: -kv[1])
        out.append(
            "**Distribution:** "
            + " | ".join(f"{d} {c}" for d, c in parts),
        )
        out.append("")

    out.append("---")
    out.append("")
    out.append("## Summary by Check")
    out.append("")
    out.append("| # | Check | Count | Files | Rule |")
    out.append("|---|-------|------:|------:|------|")
    for i, check in enumerate(active, 1):
        cnt = len(rel[check.name])
        fc = file_counts[check.name]
        out.append(
            f"| {i} | {check.name} | {cnt} | {fc} | {check.description} |",
        )
    out.append("")
    out.append("---")
    out.append("")

    section = 0
    for check in active:
        vlines = rel[check.name]
        if not vlines:
            continue
        section += 1
        out.append(f"## {section}. {check.name} ({len(vlines)} findings)")
        out.append("")
        out.append(f"{check.description}.")
        out.append("")

        files_with_counts: dict[str, int] = defaultdict(int)
        for v in vlines:
            files_with_counts[_violation_filepath(v)] += 1
        for fp, fc in sorted(
            files_with_counts.items(), key=lambda kv: (-kv[1], kv[0]),
        ):
            out.append(f"- [ ] `{fp}` \u2014 {fc}")
        out.append("")

        out.append("<details>")
        out.append("<summary>Violation details</summary>")
        out.append("")
        out.append("```")
        prev_file = ""
        for v in sorted(vlines):
            cur = _violation_filepath(v)
            if cur != prev_file:
                if prev_file:
                    out.append("")
                prev_file = cur
            out.append(v)
        out.append("```")
        out.append("")
        out.append("</details>")
        out.append("")
        out.append("---")
        out.append("")

    if total_violations == 0:
        out.append("**Result:** All checks passed.")
    else:
        out.append(
            f"**Result:** {total_violations} violation(s) in "
            f"{failed_checks} check(s). Fix all items above, then re-run:",
        )
        out.append("")
        out.append("```bash")
        out.append("scripts/check-invariants.sh --checklist")
        out.append("```")

    Path(checklist_file).write_text("\n".join(out) + "\n", encoding="utf-8")


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------


def _fixup_checklist_arg(args: argparse.Namespace) -> None:
    """Move a non-.md/.txt value back to paths (matches old bash heuristic)."""
    if (
        args.checklist is not None
        and args.checklist != "invariant-checklist.md"
        and not args.checklist.endswith((".md", ".txt"))
    ):
        args.paths.insert(0, args.checklist)
        args.checklist = "invariant-checklist.md"


def main() -> int:
    args = _parse_args()
    _fixup_checklist_arg(args)

    if args.list:
        print("Available invariant checks:\n")
        for c in CHECK_DEFS:
            print(f"  {c.name:<22s} {c.description}")
        return 0

    active = _select_checks(args.checks)

    files = discover_files(
        paths=args.paths or None,
        src_only=args.src_only,
        tests_only=args.tests_only,
    )
    if not files.all_files:
        if not args.quiet:
            print("No C++ files to check.")
        return 0

    header_set = frozenset(str(p) for p in files.all_headers)
    results = _run_checks(
        active,
        files.all_files,
        header_set,
        progress=not args.quiet or args.progress,
    )

    total_violations = sum(len(v) for v in results.values())
    failed_checks = sum(1 for v in results.values() if v)

    _print_console(
        active,
        results,
        total_violations,
        failed_checks,
        quiet=args.quiet,
        checklist_mode=args.checklist is not None,
    )

    if args.checklist is not None:
        scope_label = _build_scope_label(args)
        project_root = str(SCRIPT_DIR.parent.resolve())
        _generate_checklist(
            args.checklist,
            active,
            results,
            total_violations,
            failed_checks,
            scope_label,
            project_root,
        )
        if not args.quiet:
            if total_violations == 0:
                print(f"PASS: 0 violations. Checklist: {args.checklist}")
            else:
                print(
                    f"FAIL: {total_violations} violations in "
                    f"{failed_checks} checks. Checklist: {args.checklist}",
                )

    return 0 if total_violations == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
