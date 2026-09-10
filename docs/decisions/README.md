# Architectural Decision Records (ADRs)

Significant design choices for Simplish: what was chosen, what was rejected, and what it costs.

**Purpose:** stop re-litigating settled questions. An ADR records the alternatives that were considered so nobody — human or agent — spends a week rediscovering why an obvious-looking approach does not work here.

## Format

Every ADR follows [TEMPLATE.md](TEMPLATE.md). File naming: `ADR-NNN-short-title.md`.

## When to Write One

An ADR is required when a change:

- Chooses between two or more viable architectural patterns
- Rejects a commonly expected approach (not using an ECS, not using rollback netcode)
- Introduces a cross-cutting constraint (no exceptions, no wall-clock reads in simulation)
- Changes or supersedes a prior ADR

An ADR is *not* required for implementation detail with one obvious answer, or for choices reversible in an afternoon.

## Status Values

- **Proposed** — under discussion, not yet binding
- **Accepted** — binding; code is expected to conform
- **Superseded by ADR-NNN** — kept for the record, no longer binding

## Index

| ADR | Title | Status | Date |
|-----|-------|--------|------|
| [ADR-001](ADR-001-no-exceptions.md) | No C++ exceptions or RTTI | Accepted | 2026-08-22 |
| [ADR-002](ADR-002-fixed-timestep-determinism.md) | Fixed-timestep deterministic simulation | Accepted | 2026-08-22 |
| [ADR-003](ADR-003-hybrid-iso-render-model.md) | Hybrid 3D geometry and billboarded sprites under one depth buffer (amended 2026-09-10: skinned meshes for a handful of characters) | Accepted | 2026-08-22 |
| [ADR-004](ADR-004-soa-pools-over-ecs.md) | SoA pools with generational handles over an ECS framework | Accepted | 2026-08-22 |
| [ADR-005](ADR-005-deterministic-lockstep-coop.md) | Deterministic lockstep for co-op | Accepted | 2026-08-22 |
| [ADR-006](ADR-006-headless-deterministic-ci.md) | Stub RHI backend as the determinism CI path | Accepted | 2026-08-22 |
| [ADR-007](ADR-007-json-authored-cpp-baked-content.md) | JSON-authored content, baked to generated C++ for shipping builds | Accepted | 2026-09-03 |
| [ADR-008](ADR-008-level-scenario-hierarchy.md) | Levels own the space, scenarios sequence stages, groups select what varies | Proposed | 2026-09-09 |
