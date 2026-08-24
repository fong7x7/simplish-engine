#!/usr/bin/env python3
"""Shared file-discovery module for Simplish check scripts.

Usage as a library:
    from find_sources import discover_files, parse_common_args

Usage as a CLI (for debugging):
    python3 scripts/lib/find_sources.py [--src-only|--tests-only] [PATH...]
"""

from __future__ import annotations

import argparse
import os
import sys
from dataclasses import dataclass, field
from pathlib import Path

PRUNE_DIRS = {"build", "_deps", "third_party", "CMakeFiles", ".claude", ".git",
              ".cache", "node_modules"}

# All buildable code lives under src/, one package per directory, each with
# include/, src/, and test/ (docs/development/code-layout.md). There is no
# top-level tests/ tree — tests sit inside the package they cover, so test
# files are identified by a path component rather than by a root directory.
SOURCE_ROOTS = ["src"]
TEST_DIR_NAMES = {"test", "tests"}

# Objective-C++ is a first-class source kind here: the Metal backend is .mm.
SOURCE_SUFFIXES = (".cpp", ".mm")
HEADER_SUFFIXES = (".h", ".hpp")


@dataclass
class SourceFiles:
    """Categorized lists of discovered C++ source files."""

    src_headers: list[Path] = field(default_factory=list)
    src_sources: list[Path] = field(default_factory=list)
    test_headers: list[Path] = field(default_factory=list)
    test_sources: list[Path] = field(default_factory=list)

    @property
    def all_src(self) -> list[Path]:
        return self.src_headers + self.src_sources

    @property
    def all_test(self) -> list[Path]:
        return self.test_headers + self.test_sources

    @property
    def all_files(self) -> list[Path]:
        return self.all_src + self.all_test

    @property
    def all_headers(self) -> list[Path]:
        return self.src_headers + self.test_headers

    @property
    def all_sources(self) -> list[Path]:
        return self.src_sources + self.test_sources


def _project_root() -> Path:
    """Find the project root by walking up from this file's location."""
    p = Path(__file__).resolve().parent
    while p != p.parent:
        if (p / "CMakeLists.txt").exists() and (p / "src").is_dir():
            return p
        p = p.parent
    raise RuntimeError(
        "Cannot find project root (looked for a directory holding both "
        "CMakeLists.txt and src/)"
    )


def _should_prune(dirpath: str, dirnames: list[str]) -> list[str]:
    """Remove pruned directory names in-place and return the kept list."""
    kept = [d for d in dirnames if d not in PRUNE_DIRS]
    dirnames[:] = kept
    return kept


def _collect_cpp_files(root: Path) -> tuple[list[Path], list[Path]]:
    """Walk a directory tree and return (headers, sources)."""
    headers: list[Path] = []
    sources: list[Path] = []
    for dirpath, dirnames, filenames in os.walk(root):
        _should_prune(dirpath, dirnames)
        for f in sorted(filenames):
            p = Path(dirpath) / f
            if f.endswith(HEADER_SUFFIXES):
                headers.append(p)
            elif f.endswith(SOURCE_SUFFIXES):
                sources.append(p)
    return headers, sources


def _is_under_tests(path: Path, project_root: Path) -> bool:
    """Check whether a path lies inside a package's test directory.

    Tests live beside the code they cover (``src/<layer>/<package>/test/``),
    so this matches any ``test``/``tests`` component rather than a single
    top-level root.
    """
    try:
        rel = path.resolve().relative_to(project_root.resolve())
    except ValueError:
        return False
    return any(part in TEST_DIR_NAMES for part in rel.parts)


def discover_files(
    paths: list[str] | None = None,
    src_only: bool = False,
    tests_only: bool = False,
    project_root: Path | None = None,
) -> SourceFiles:
    """Discover C++ files matching the given criteria.

    Args:
        paths: Specific files or directories to search. If empty/None, searches
               all source and test roots.
        src_only: Only include production source files.
        tests_only: Only include test files.
        project_root: Override project root detection.

    Returns:
        SourceFiles with categorized file lists.
    """
    root = project_root or _project_root()
    result = SourceFiles()

    if not paths:
        for src_root in SOURCE_ROOTS:
            d = root / src_root
            if not d.is_dir():
                continue
            headers, sources = _collect_cpp_files(d)
            for f in headers + sources:
                is_test = _is_under_tests(f, root)
                if is_test and src_only:
                    continue
                if not is_test and tests_only:
                    continue
                header = f.suffix in HEADER_SUFFIXES
                if is_test:
                    (result.test_headers if header
                     else result.test_sources).append(f)
                else:
                    (result.src_headers if header
                     else result.src_sources).append(f)
    else:
        for raw in paths:
            p = Path(raw)
            if not p.is_absolute():
                p = root / p
            p = p.resolve()

            if not p.exists():
                print(f"Error: not found: {raw}", file=sys.stderr)
                sys.exit(1)

            if p.is_file():
                if p.suffix not in HEADER_SUFFIXES + SOURCE_SUFFIXES:
                    kinds = "/".join(HEADER_SUFFIXES + SOURCE_SUFFIXES)
                    print(
                        f"Error: not a C++ file ({kinds}): {raw}",
                        file=sys.stderr,
                    )
                    sys.exit(1)
                files = [p]
            else:
                h, s = _collect_cpp_files(p)
                files = h + s

            for f in files:
                is_test = _is_under_tests(f, root)
                if is_test and src_only:
                    continue
                if not is_test and tests_only:
                    continue

                header = f.suffix in HEADER_SUFFIXES
                if is_test:
                    (result.test_headers if header
                     else result.test_sources).append(f)
                else:
                    (result.src_headers if header
                     else result.src_sources).append(f)

    result.src_headers.sort()
    result.src_sources.sort()
    result.test_headers.sort()
    result.test_sources.sort()
    return result


def parse_common_args(
    description: str,
    extra_args: list[tuple] | None = None,
) -> argparse.Namespace:
    """Parse common arguments shared by all check scripts.

    Args:
        description: Script description for --help.
        extra_args: Additional (flags, kwargs) tuples to add to the parser.

    Returns:
        Parsed namespace with .paths, .src_only, .tests_only attributes.
    """
    parser = argparse.ArgumentParser(description=description)
    group = parser.add_mutually_exclusive_group()
    group.add_argument(
        "--src-only", action="store_true", help="Check only production code"
    )
    group.add_argument(
        "--tests-only", action="store_true", help="Check only test code"
    )
    parser.add_argument(
        "paths", nargs="*", help="Specific files or directories to check"
    )
    if extra_args:
        for flags, kwargs in extra_args:
            if isinstance(flags, str):
                flags = [flags]
            parser.add_argument(*flags, **kwargs)
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_common_args("List discovered C++ source files")
    files = discover_files(
        paths=args.paths or None,
        src_only=args.src_only,
        tests_only=args.tests_only,
    )
    for category, flist in [
        ("src_headers", files.src_headers),
        ("src_sources", files.src_sources),
        ("test_headers", files.test_headers),
        ("test_sources", files.test_sources),
    ]:
        if flist:
            print(f"\n{category} ({len(flist)}):")
            for f in flist:
                print(f"  {f}")
