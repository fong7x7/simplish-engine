# Simplish — Design Principles

**Parent document:** [REQUIREMENTS.md](../../REQUIREMENTS.md)
**Last Updated:** 2026-08-22

These principles are **ranked**. When two designs both satisfy the requirements, the one that better serves the higher-ranked principle wins. Every [ADR](../decisions/README.md) cites the principles that informed it.

---

## Ranked Principles

### 1. Determinism Always

The simulation is a pure function of initial state, seed, and the ordered input stream. Same inputs, same bits, every platform, every run.

This ranks first because so much else depends on it. Co-op netcode is lockstep — cheap — only because the simulation is deterministic. Replays are exact recordings rather than approximations. A bug at 2,000 enemies is reproducible from a 100 KB file instead of unreproducible forever.

**In practice:** no wall-clock reads in simulation code. No hash-map iteration. No unordered parallelism. No fast-math. Named RNG streams, with cosmetic streams excluded from the state hash. If a feature cannot be made deterministic, it moves out of the simulation or it does not ship.

### 2. Budgets Are Requirements

Every subsystem has a stated per-frame cost, measured in CI, and exceeding it fails the build.

A horde shooter is a performance problem wearing a game costume. Performance discovered late is performance that requires a rewrite, so the budget is part of the specification from the first commit — not a tuning pass before ship.

**In practice:** design against the numbers in [Engine §7](../engine/REQUIREMENTS.md#7-non-functional-requirements). When a design cannot meet its budget, change the design rather than the budget — and if the budget genuinely was wrong, change it deliberately, in the document, with the reason recorded.

### 3. Data-Oriented Over Object-Oriented

Layout for the access pattern. Dense arrays, linear iteration, batch operations. Structure of arrays where the loop touches a subset of fields.

At four entities, layout is irrelevant. At 20,000 projectiles, layout *is* the performance.

**In practice:** SoA pools with generational handles, not object graphs. No virtual dispatch in per-entity hot loops. No per-entity allocation during a run. Transform an array, not an object.

### 4. Simplicity Over Flexibility

Build what the requirements state. A generalisation that no requirement asks for is a cost with no offsetting benefit — and it constrains the code that comes after it.

The camera never rotates. Say so, and let the renderer depend on it. Half of what makes this engine fast is the generality it declined to have.

**In practice:** no plugin system until a requirement needs one. No abstraction with a single implementation. No configuration option nobody sets. Delete speculative code paths.

### 5. Explicit Over Implicit

State intent in the code. Ordering, ownership, lifetime, and units are visible at the call site, not inferred from context.

**In practice:** the tick phase order is written down and enforced. Handles carry generations so a stale reference is caught, not silently valid. Units live in type names or parameter names. `RhiDeviceFactory::create()` returns an optional; failure is in the signature, not a convention.

### 6. Testability by Construction

A system that can only be tested by running the game is a system that will not be tested.

**In practice:** simulation systems run headless, with no GPU and no window. Content is data, so a test can build a scenario without a level file. The director is a pure function of state and seed, so its pacing is unit-testable. Stub backend, deterministic clock, injectable RNG.

### 7. Platform-Agnostic by Default

Code is platform-agnostic unless it is in `platform/`. `engine/` and `game/` compile once and link everywhere.

**In practice:** no SDL, no graphics API, no distributor SDK outside `platform/`. Abstract interfaces defined by the layer that consumes them. Platform types never cross a public header.

### 8. Legibility Is a Feature

The player must be able to read a screen holding 2,000 enemies and 20,000 projectiles. Where fidelity and readability conflict, readability wins.

This is ranked last not because it matters least, but because it is a design constraint rather than an engineering one — it binds art and effects work more than architecture. It is here so that no one treats it as negotiable.

**In practice:** reserved hue bands for hostile projectiles. Silhouette-first enemy identification. Effect budgets that cull cosmetics before gameplay-relevant signals. Occluders fade rather than vanish.

---

## How to Use These Principles

**Designing.** Check your approach against the principles in order. A design that serves principle 3 at the expense of principle 1 is wrong — determinism outranks layout, so find the layout that is both.

**Reviewing.** Cite the principle. "This iterates an unordered map in the tick — principle 1" is a complete review comment.

**Writing an ADR.** The `Design Principle References` section is required. If a decision cites no principle, either the principle set is incomplete or the decision is arbitrary; both are worth knowing.

**Disagreeing.** The ranking is a tool, not scripture. If a principle is consistently getting in the way of good work, that is an argument for changing the ranking — made explicitly, in a pull request, not silently in code.
