# Solutions Library

Documented solutions from [compound engineering](../development/compound-engineering.md) workflows. Each file captures a problem, its root cause, the solution, and how to prevent it — so the same problem is never solved twice.

These are consumed by agents via frontmatter search during the planning phase, not read as documentation pages. That is why they live here rather than alongside the requirements documents: a solution is a dated, narrow artifact, not a normative one.

## How It Works

When you solve a non-trivial problem, run `ce-compound` to create a solution document here. During planning, `ce-plan`'s learnings researcher searches these files by frontmatter metadata and surfaces relevant past solutions before you start implementing.

The test for whether something belongs here: **would this save someone 30+ minutes if they hit it again?**

## Template

```yaml
---
title: "Descriptive Problem Title"
category: <the directory this file lives in — see Categories below>
tags: [relevant, searchable, terms]
date: YYYY-MM-DD
---

## Problem

What went wrong or what needed to be built.

## Root Cause

Why the problem occurred (for bugs) or why it's non-obvious (for features).

## Solution

What was done to fix or implement it, with code examples.

## Prevention

How to prevent this class of problem in the future.
```

Those four frontmatter fields are the floor and the four body headings are the spine. Everything past that is optional.

**Optional fields worth adding** when they are what you would search on: `severity`, `platform` (which of macOS/Windows/Linux/PS5/Xbox), `backend` (which RHI backend), `component`, `symptoms`, `resolution_type`, `verified`, `time_to_resolve`. There is no fixed schema and no check validates these — add a field because you would filter on it, not for completeness.

**Do not add a `problem_type` field.** It duplicates `category`, which is the directory name and part of the floor. Frontmatter search is this directory's interface, and a field whose values disagree with each other returns worse results than no field at all.

## Categories

One directory per category; `category:` in the frontmatter matches the directory name.

| Directory | Contents |
|---|---|
| `determinism/` | Desyncs, cross-platform float divergence, tick-order dependencies, RNG stream mistakes, replay mismatches |
| `rendering/` | RHI backend behaviour differences, depth and sorting bugs, sprite/geometry interleave, shader issues, golden-image failures |
| `performance/` | Budget regressions with before/after numbers, cache behaviour, draw-call and batching findings, allocation in hot paths |
| `platform/` | Platform-specific breakage: SDL3, windowing, macOS/Windows/Linux differences, console SDK traps |
| `build-issues/` | CMake, presets, toolchain, linking, dependency fetching, cross-compilation |
| `memory/` | Leaks, pool sizing, lifetime bugs, generational-handle misuse, sanitizer findings |
| `gameplay-logic/` | Director pacing, modifier stacking, damage resolution, horde steering, spawn budget |
| `testing/` | Flaky tests, headless-run traps, fixture design, golden-image tolerance, determinism test technique |
| `architecture/` | Layering violations, dependency-direction mistakes, package boundary decisions that turned out wrong |
| `tooling/` | Editor, asset pipeline, profiler, dev console, CI workflow |

## What Not to Put Here

| This belongs in | Not here, because |
|---|---|
| [`docs/decisions/`](../decisions/README.md) | You chose between viable architectures. That is prospective and binding; a solution is retrospective and narrow |
| `docs/*/REQUIREMENTS.md` | The finding changes what must be true about the system. Promote it |
| [Design principles](../development/design-principles.md) | The finding is a rule that should govern all future work |

A solution document that starts telling everyone how they must write code has outgrown this directory.
