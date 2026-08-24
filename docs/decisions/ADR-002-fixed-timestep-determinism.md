# ADR-002: Fixed-timestep deterministic simulation

**Status:** Accepted
**Date:** 2026-08-22
**Scope:** Engine | Game

## Context

Three separate requirements all want the same thing. Co-op needs the four peers to agree on the state of thousands of entities without shipping that state over the wire. Replays need to reproduce a run exactly, both as a feature and as a debugging tool. And a bug that only appears with 2,000 enemies and 20,000 projectiles in flight is not reproducible by description — it needs a recording that replays into the same failure.

Determinism delivers all three, but it is not something that can be retrofitted. It is a property of every line of simulation code, and a codebase that did not start with it will not acquire it later.

## Decision

The simulation is a pure function of `(initial state, seed, ordered input stream)`, advancing in fixed 16.667 ms (60 Hz) ticks. Rendering is decoupled and interpolates between the two most recent states.

The contract, enforced by review and by CI:

- **Fixed tick, fixed order.** The phase order in [Engine §4.1](../engine/REQUIREMENTS.md#41-fixed-timestep) is part of the contract. The accumulator is clamped to 4 ticks per frame and the shortfall is reported.
- **No wall-clock reads.** `now()` is not linked into simulation translation units.
- **Deterministic iteration.** No hash-map iteration in a tick. Associative lookups are backed by sorted dense arrays.
- **Named RNG streams.** PCG32, one stream per system. Cosmetic streams are excluded from the state hash so visual variance cannot desync a session.
- **Strict floating point.** `-ffp-contract=off`, `/fp:precise`, no fast-math, no libm transcendentals in simulation code — the engine supplies deterministic replacements.
- **Scalar simulation.** SIMD is permitted in rendering and audio; simulation stays scalar unless a SIMD path is proven bit-identical across architectures.
- **Tick hashing.** Debug and CI builds hash all simulation state per tick. Divergence is caught at the tick it occurs, with the offending subsystem named.

## Alternatives Considered

### Alternative A: Variable timestep with delta time

- **How it works:** Each frame advances the simulation by its measured duration.
- **Pros:** Simple; no accumulator; render and simulation always aligned.
- **Cons:** Kills determinism outright — frame timing varies per machine and per run. Replays become approximations. Lockstep becomes impossible, forcing state replication or rollback. Physics behaviour becomes frame-rate dependent, which is a gameplay bug at horde scale.

### Alternative B: Fixed timestep without a strict determinism contract

- **How it works:** Fixed ticks for stable physics, but no restrictions on wall-clock reads, container iteration, or floating-point flags.
- **Pros:** Most of the physics benefit, none of the discipline.
- **Cons:** Yields *approximate* reproducibility, which is worse than none — it works in testing and diverges in the field, at tick 40,000, after the interesting part. Netcode cannot rely on it, so the replication cost returns anyway.

### Alternative C: Fixed-point arithmetic throughout

- **How it works:** Replace floats with fixed-point integers in simulation code.
- **Pros:** Bit-identical by construction, no floating-point flag discipline needed.
- **Cons:** Invasive, error-prone in range and precision, and hostile to the collision and steering math. Strict IEEE-754 with contraction disabled is already deterministic across the target architectures; this buys correctness we can obtain more cheaply. Revisit only if cross-platform hash divergence proves otherwise in CI.

## Design Principle References

- **Principle 1: Determinism Always** — this ADR *is* the principle, made concrete.
- **Principle 6: Testability by Construction** — a deterministic simulation is testable headless, and its bugs arrive as reproducible artifacts.
- **Principle 5: Explicit Over Implicit** — a written, enforced phase order beats an ordering that emerges from call sites.

## Consequences

### Positive

- Lockstep co-op becomes affordable ([ADR-005](ADR-005-deterministic-lockstep-coop.md)) — inputs on the wire, not state.
- Replays are exact, tiny, always-on, and shippable with crash reports.
- Horde-scale bugs are reproducible from a small file.
- Frame-rate-independent gameplay by construction.
- Cross-platform hash comparison in CI catches whole classes of bug that no unit test would.

### Negative

- Every simulation system carries the discipline: no time reads, no unordered iteration, no casual SIMD.
- Third-party libraries are largely unusable inside the simulation unless their determinism is verified.
- Rendering needs interpolation, which adds render-side complexity and up to one tick of visual latency.
- A tick that overruns its budget causes visible stutter rather than degrading smoothly — which is the point, but it makes [budget enforcement](../development/REQUIREMENTS.md#6-performance-gates) non-negotiable.

### Implications for Future Work

- Every new simulation system needs a determinism test, not only a unit test.
- Any parallelism inside the tick must produce results independent of scheduling — deterministic work partitioning with fixed merge order, or it stays serial.
- Audio, particles, and screen shake read simulation state but never write it, and use FX RNG streams excluded from the hash.
- Cross-platform tick-hash agreement is a release blocker ([Development §7](../development/REQUIREMENTS.md#7-ci-matrix)).
