# ADR-001: No C++ exceptions or RTTI

**Status:** Accepted
**Date:** 2026-08-22
**Scope:** Engine | Game | Editor | Platform | Build

## Context

C++ exceptions carry costs that do not fit this project. The unwinding machinery inflates binary size and inhibits some optimisations; more importantly, an exception is an invisible control-flow edge out of every call, which makes reasoning about a hot loop harder and makes reasoning about *deterministic* ordering harder still. Console platforms have historically discouraged or disallowed them outright.

The copied platform layer already assumes their absence: `RhiDeviceFactory::create()` returns `std::optional<std::unique_ptr<RhiDevice>>` and every distributor header describes failure as a returned value, not a thrown type.

RTTI carries a smaller but similar cost — vtable bloat and a temptation toward `dynamic_cast`-driven designs that the data-oriented layout in [ADR-004](ADR-004-soa-pools-over-ecs.md) rules out anyway.

## Decision

Build with `-fno-exceptions -fno-rtti` (`/EHs-c- /GR-` on MSVC) across every target. Errors are communicated by return value: `std::optional` for "this may not exist", `std::expected`-style result types for "this failed and here is why", and an assertion for a violated invariant.

Third-party libraries that throw internally are wrapped at the boundary, with the wrapper compiled with exceptions enabled and converting any escape into a return value.

## Alternatives Considered

### Alternative A: Exceptions for exceptional cases only

- **How it works:** Enable exceptions; use them for genuinely unrecoverable conditions (allocation failure, asset corruption) and return values elsewhere.
- **Pros:** Idiomatic modern C++; standard-library types work without qualification.
- **Cons:** "Exceptional only" erodes in practice. The unwinding cost is paid across the whole binary regardless of how rarely it fires. Console friction remains. And the invisible control-flow edges are exactly what makes a deterministic tick harder to audit.

### Alternative B: Exceptions in tooling, disabled in runtime

- **How it works:** The editor and offline tools build with exceptions; engine and game do not.
- **Pros:** Tooling gets convenient error handling where performance is irrelevant.
- **Cons:** Two ABI variants of every shared target, and the editor links the engine and game directly. The split costs more than it saves.

## Design Principle References

- **Principle 5: Explicit Over Implicit** — a function's failure mode is in its signature, not in a convention documented elsewhere.
- **Principle 1: Determinism Always** — control flow that is visible in the source is control flow that can be audited for ordering.
- **Principle 2: Budgets Are Requirements** — no unwinding tables, smaller binaries, fewer optimisation barriers.

## Consequences

### Positive

- Failure handling is visible at every call site.
- Smaller binaries, no unwind tables, no hidden exit paths in hot loops.
- Console-friendly from the start.
- The existing platform layer already conforms — zero migration cost.

### Negative

- Standard-library constructs that signal by throwing (`std::stoi`, `.at()`, allocating containers under memory pressure) need alternatives or wrappers.
- Constructors cannot fail. Types that can fail to initialise need a static factory returning an optional — the pattern `RhiDeviceFactory` already uses.
- Third-party integration sometimes needs a wrapper translation unit.

### Implications for Future Work

- Every fallible operation returns a value. Reviewers reject constructors that can fail.
- The engine provides non-throwing replacements for the standard-library facilities it needs.
- Allocation failure is fatal by policy: the engine asserts and terminates rather than attempting recovery, since the pool-based allocation model in [ADR-004](ADR-004-soa-pools-over-ecs.md) makes mid-run allocation failure a bug rather than a condition.
