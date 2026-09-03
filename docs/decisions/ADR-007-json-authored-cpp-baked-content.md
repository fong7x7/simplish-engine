# ADR-007: JSON-authored content, baked to generated C++ for shipping builds

**Status:** Accepted
**Date:** 2026-09-03
**Scope:** Editor | Engine | Build

## Context

The editor authors levels, encounters, scenarios, trigger logic, and data tables ([Editor §4–§6](../editor/REQUIREMENTS.md#4-level-authoring)). All of it has to reach the running game, and the path it takes decides four things at once: how fast a designer can iterate, how fast the game loads, whether content mistakes are caught at build time or at 3am in a playtest, and whether two machines in a lockstep co-op session agree.

Three existing decisions constrain the answer before it is asked.

**Determinism is first ([ADR-002](ADR-002-fixed-timestep-determinism.md), Principle 1).** Content is an input to the simulation. If two builds disagree about a weapon's damage or the order in which a trigger's conditions evaluate, lockstep co-op ([ADR-005](ADR-005-deterministic-lockstep-coop.md)) desyncs and replays stop reproducing. Whatever the pipeline does, it must do it identically everywhere.

**Playtest is the editor's load-bearing feature ([Editor §7](../editor/REQUIREMENTS.md#7-playtest)).** It runs "from the current edit state — unsaved changes included". A designer presses play and the game starts. Any pipeline stage between editing and playing has to be measured against that sentence.

**Content is specified as hot-reloadable** ([Engine §3](../engine/REQUIREMENTS.md), M3): "JSON data tables + in-tree schema validator, hot-reload in debug builds". Data edits are meant to reach a running session where it is safe to apply them.

The requirement driving this ADR is that shipping builds should carry content as compiled C++ rather than parsing JSON at startup. That is in direct tension with the two requirements above: a compile step cannot sit between a designer pressing play and the game running.

## Decision

**JSON is the single source of truth. Shipping builds compile generated C++ produced from it. Editor and development builds load the JSON directly, through the same in-memory types.**

- The editor writes and reads JSON only. No editor operation produces a binary or generated artifact, and no generated artifact is ever hand-edited or committed — it lives in the build directory.
- A generator (`simplish-content-gen`) turns a validated project into C++ translation units at build time, as a CMake step ahead of the game target.
- The runtime has **two loaders producing one representation**: `loadFromJson` and the generated `contentTables()`. Everything downstream — simulation, director, renderer — sees the same structs and cannot tell which path filled them.
- **Playtest and hot-reload use the JSON path, always.** They are dev-build features and never require the generator to have run.
- **Shipping builds contain no JSON parser for content and no content files.** The generated tables are the only content that exists, statically initialised, with no load step to fail.
- An equivalence test loads a corpus of projects through both paths and asserts the resulting tables are bit-identical. Without it the two paths drift, and the drift shows up as a desync between a developer and a player rather than as a failing build.

Expressions in trigger and scenario logic get the same dual treatment: interpreted by `ExpressionEvaluator` on the JSON path, emitted as real C++ expressions on the generated path. Their grammar is therefore restricted to constructs with identical semantics in both — which is a real constraint on the format, spelled out in [project-format.md](../editor/project-format.md#7-logic-and-expressions).

## Alternatives Considered

### Alternative A: Parse JSON at runtime, everywhere

- **How it works:** Ship the JSON. The runtime parses and validates it at load.
- **Pros:** One code path, so no possibility of dev and ship disagreeing. No build step, no generator, no CMake integration. Content patches without a recompile.
- **Cons:** Every reference stays a string resolved at load — a typo in an enemy archetype id is a runtime error on the level that uses it, found by whoever loads that level, rather than a compile error. Load time scales with content and is paid on every launch on every platform. Values that are constant for the life of the process still cost an indirection the optimiser cannot see through. Ships a JSON parser and the content files into the shipping binary, which is both attack surface and something to tamper with.

### Alternative B: Bake to a binary blob

- **How it works:** The build step writes a packed binary the runtime memory-maps and casts into place.
- **Pros:** Fast to load and compact. No compile-time cost for large content. Keeps content out of the compiler entirely.
- **Cons:** All of codegen's build complexity and none of its type safety: a format version mismatch is a crash or, worse, silently misread data. Needs its own versioning, endianness, and alignment discipline — a second serialisation format to maintain beside the JSON one. Nothing is constant-folded; the compiler still cannot see the values. It trades the compiler's checking for a loader nobody wants to write twice.

### Alternative C: Embed a scripting language for logic

- **How it works:** Scenario and trigger logic authored in Lua or similar, interpreted at runtime.
- **Pros:** Expressive, familiar, hot-reloadable by nature.
- **Cons:** A VM inside the deterministic tick is a determinism hazard the project cannot afford — floating-point behaviour, table iteration order, and garbage collection timing all vary. It also contradicts Principle 4: the requirements ask for authored triggers and conditions, not a general programming environment. `ExpressionEvaluator` already covers the stated need with no loops, no side effects, and bounded evaluation.

### Alternative D: Codegen only, with no JSON loader at all

- **How it works:** The chosen approach, minus the runtime JSON path — the editor regenerates and recompiles to playtest.
- **Pros:** One representation in the runtime. No equivalence test, no drift.
- **Cons:** It breaks the editor's central feature. Playtest becomes a build, and iteration goes from a keystroke to a compile — for content changes that are meant to apply *during* a session. This is the reason the decision is dual-path rather than pure codegen, and it is not a close call.

## Design Principle References

- **Principle 1: Determinism Always** — generated output is byte-identical for identical input, with no hash-map iteration order and no dependence on filesystem enumeration order; the equivalence test extends that guarantee across the two loaders, so a dev build and a shipping build simulate the same.
- **Principle 5: Explicit Over Implicit** — references between content files are typed ids resolved at generation time, so a dangling reference is a build failure naming both files, not a null at runtime.
- **Principle 6: Testability by Construction** — content is data, so a test builds a scenario without a level file; the JSON path is what makes that possible in a headless test with no generator step.
- **Principle 4: Simplicity Over Flexibility** — logic is declarative triggers over a restricted expression grammar, not an embedded language, because that is what the requirements ask for.

## Consequences

### Positive

- Content mistakes move to build time. An unknown archetype id, an out-of-range spawn weight, or a trigger referencing a deleted region fails the build with a file and line, rather than reaching a player.
- Shipping builds have no content load step and no content parse failure mode. Tables are in `.rodata`; startup does not touch them.
- The compiler sees content as constants: values fold, switches over generated enums are dense, and per-entity lookups become array indexing rather than string hashing.
- Content ships as machine code, not as readable files beside the executable.

### Negative

- **Two loaders to keep honest.** The equivalence test is not optional; without it the paths drift silently. It is the price of keeping playtest instant.
- **Build time scales with content volume.** Large levels are the pathological case: a tile grid emitted as a C++ initializer list is minutes of compiler time. The format keeps bulk payloads out of the compiler's way (see below), but this needs measuring as levels grow, and it is the most likely reason a future ADR revisits this one.
- **Content changes require a rebuild to ship.** Fine for a designer, who works in the editor against the JSON path; it does mean no content hotfix without a patch.
- **The expression grammar is constrained** to what evaluates identically interpreted and compiled. Some conveniences cannot be added to it later without breaking that equality.

### Implications for Future Work

- Bulk arrays — tile layers above a few thousand cells — are emitted as `constexpr` byte arrays in their own translation unit, so recompiling a level does not recompile the logic that reads it, and the compiler's slowest case is isolated where it can be measured.
- The generator is a host tool with no engine dependency beyond the schema headers, so it cross-compiles trivially and runs identically on every CI leg.
- Content generation joins the determinism CI job ([ADR-006](ADR-006-headless-deterministic-ci.md)): generate twice, diff the output, fail on any difference.
- If build times become the binding constraint, Alternative B is the fallback for bulk payloads specifically — a binary blob for tile data, generated C++ for everything else — and this ADR should be superseded rather than quietly eroded.
