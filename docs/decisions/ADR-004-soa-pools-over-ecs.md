# ADR-004: SoA pools with generational handles over an ECS framework

**Status:** Accepted
**Date:** 2026-08-22
**Scope:** Engine | Game

## Context

The engine must hold roughly 2,000 active enemies and 20,000 in-flight projectiles, iterate them every tick within a 6 ms budget, and do so in a bit-identical order on every platform.

The default modern answer is an entity-component-system framework — EnTT, flecs, or a hand-rolled equivalent. ECS gets the data layout right and composes behaviour flexibly. But its flexibility is bought with archetype tables, dynamic queries, and a scheduler, and each of those introduces ordering that depends on registration order, archetype creation order, or a dependency graph the framework resolves at runtime. That is exactly the class of hidden ordering that [ADR-002](ADR-002-fixed-timestep-determinism.md) forbids.

The countervailing fact is that this game's entity taxonomy is small and known: players, a handful of enemy archetypes, projectiles, deployables, pickups, hazards. The composition problem ECS solves elegantly is a problem this game barely has.

## Decision

Entities live in **structure-of-arrays pools, one per archetype family**, indexed by generational handles (`{index: u32, generation: u32}`).

- Fields are stored as parallel dense arrays, so a system iterating three fields touches three contiguous streams and nothing else.
- Handles carry a generation counter; a stale handle is detected on dereference rather than silently pointing at a recycled slot.
- Destruction is deferred to an explicit compaction phase at the end of the tick, so indices are stable for the tick's duration.
- Pools are sized from level metadata at load. No per-entity heap allocation occurs during a run.
- Behaviour composes by struct: an archetype's pool contains the fields its behaviours need. Shared behaviour is a free function over the arrays it reads and writes.
- Systems run in the fixed order written in [Engine §4.1](../engine/REQUIREMENTS.md#41-fixed-timestep). There is no scheduler, and there is no dependency resolution — the order is source code.

## Alternatives Considered

### Alternative A: A third-party ECS (EnTT, flecs)

- **How it works:** Entities are IDs; components are registered types; systems are queries over component sets, ordered by a scheduler.
- **Pros:** Excellent cache behaviour, mature, well-tested, composes new entity kinds with no boilerplate.
- **Cons:** Iteration order depends on archetype layout, which depends on component registration and creation order — auditable in principle, fragile in practice, and precisely the hidden ordering the determinism contract rules out. Query and scheduler machinery is a runtime cost for flexibility this taxonomy does not need. Deterministic entity ID allocation across peers needs care the framework does not provide by default.

### Alternative B: Object-oriented entity hierarchy

- **How it works:** A base `Entity` class with virtual `update()`, subclassed per archetype, held in a polymorphic container.
- **Pros:** Familiar; behaviour lives with its data; trivially extensible.
- **Cons:** Virtual dispatch and pointer chasing per entity per tick, at 22,000 entities. Heap allocation per entity. Cache behaviour that fails at exactly the scale that matters. Rejected on the budget alone.

### Alternative C: A single generic SoA pool with runtime component masks

- **How it works:** One pool, all possible fields, a bitmask per entity for which are live.
- **Pros:** Uniform storage; one code path; no per-archetype boilerplate.
- **Cons:** Memory scales with the union of all fields times the entity count. Iteration touches cold fields or needs mask-driven branching that defeats the layout. Worst of both approaches.

## Design Principle References

- **Principle 3: Data-Oriented Over Object-Oriented** — layout follows the access pattern; systems transform arrays.
- **Principle 1: Determinism Always** — iteration order is array order, which is stable, visible, and platform-independent.
- **Principle 4: Simplicity Over Flexibility** — the entity taxonomy is small and known; a framework built for open-ended composition is generality without a requirement behind it.
- **Principle 5: Explicit Over Implicit** — system order is source code, not a resolved dependency graph.

## Consequences

### Positive

- Iteration order is trivially deterministic and trivially auditable.
- Linear traversal of exactly the fields a system reads.
- No allocation during a tick; memory footprint is known at level load.
- Generational handles catch use-after-free at the point of use.
- No framework dependency, no framework abstraction to learn or debug.

### Negative

- Adding an archetype family means writing a pool: field arrays, spawn, destroy, compaction. More boilerplate than an ECS component registration.
- Cross-archetype systems (damage resolution touching players, enemies, and deployables) must iterate several pools explicitly instead of expressing one query.
- Refactoring an entity's field set touches its pool definition directly rather than being absorbed by a framework.
- Sizing pools from level metadata means a level that under-declares its needs fails at load. Loud, but a real authoring constraint.

### Implications for Future Work

- A code-generation or macro helper for pool boilerplate is worth building once three or four pools exist and the shape has settled — not before.
- Cross-archetype systems need a documented iteration order across pools, since that order is part of the determinism contract.
- Pool capacity requirements belong in level metadata and must be validated by the editor before playtest ([Editor §5](../editor/REQUIREMENTS.md#5-encounter-and-wave-authoring)).
- If the entity taxonomy grows well beyond what is anticipated here, revisit this decision — but revisit it against the determinism contract, which does not become negotiable because the taxonomy grew.
