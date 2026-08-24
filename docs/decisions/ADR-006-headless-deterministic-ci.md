# ADR-006: Stub RHI backend as the determinism CI path

**Status:** Accepted
**Date:** 2026-08-22
**Scope:** Engine | Platform | Build

## Context

The determinism contract ([ADR-002](ADR-002-fixed-timestep-determinism.md)) is only worth what CI can prove about it. Proving it means running a replay corpus on macOS, Windows, and Linux every commit and comparing tick hashes bit for bit — and doing it fast enough that nobody is tempted to skip it.

Standard CI runners have no GPU worth the name. Software rasterisation is available but slow, and it introduces a component whose behaviour varies by driver version — into a job whose entire purpose is proving that nothing varies.

The copied platform layer already contains a stub backend (`platform/render/backends/stub/`) that satisfies the `RhiDevice` interface without touching hardware.

## Decision

The stub backend is a **first-class, supported configuration**, not a testing convenience. The `headless` preset resolves `ENGINE_RENDERER` to `STUB`, and the determinism, replay-corpus, and soak jobs all run on it.

- The stub implements the full `RhiDevice` and `RhiCommandList` interface, accepting and validating every call while performing no GPU work.
- It validates rather than merely ignoring: resource lifetime, command-list state transitions, and binding correctness are checked, so the headless jobs catch a class of RHI misuse that a real backend would tolerate silently or crash on obscurely.
- The simulation must be capable of running to completion with no window, no device, and no swapchain.
- Headless tick hashes must be bit-identical to those produced by any GPU backend. A divergence between headless and GPU runs is a bug in the layering — something in the simulation is reading rendering state — and is treated as a release blocker.

## Alternatives Considered

### Alternative A: Software rasterisation (llvmpipe, WARP)

- **How it works:** Run the real Vulkan or DX12 backend against a software device.
- **Pros:** Exercises the real backend code path; can produce real images for golden-image comparison.
- **Cons:** Slow enough to discourage running determinism on every commit. Behaviour varies by driver and version, introducing variance into the one job that exists to prove there is none. Solves a problem — image correctness — that the determinism job does not have.

### Alternative B: GPU-equipped CI runners

- **How it works:** Provision runners with real GPUs across all three platforms.
- **Pros:** Tests the real path end to end.
- **Cons:** Expensive and operationally awkward across three OSes. Introduces driver-version variance. And it is still the wrong tool: the determinism job tests the *simulation*, which does not touch the GPU, so a GPU is cost with no coverage.

### Alternative C: Compile the simulation without any render layer

- **How it works:** A separate build configuration that excludes rendering entirely.
- **Pros:** Fastest possible; no RHI involvement at all.
- **Cons:** A second binary shape that diverges from the shipping one, so the headless job stops testing what ships. The stub backend gets the same speed while keeping the real structure intact — and validates the RHI usage as a bonus.

## Design Principle References

- **Principle 6: Testability by Construction** — a simulation that cannot run headless will not be tested at the frequency the determinism guarantee requires.
- **Principle 7: Platform-Agnostic by Default** — the stub proves the layering is honest: if the simulation cannot run without a GPU, something has leaked across a boundary that should not have.
- **Principle 1: Determinism Always** — removing the GPU removes the largest remaining source of cross-platform variance from the job that must have none.

## Consequences

### Positive

- Determinism and replay verification run on every commit, on every platform, on ordinary runners, in seconds.
- The soak job runs long simulations cheaply, in parallel, overnight.
- The stub's validation catches RHI misuse early and portably.
- The stub is the natural target for the dedicated server build ([Project REQUIREMENTS §6](../../REQUIREMENTS.md#6-repository--project-structure-target)) — no separate headless path to maintain.
- A layering violation announces itself: if the simulation starts depending on rendering, the headless job stops working.

### Negative

- The stub is code that must be maintained in step with the `RhiDevice` interface. Every interface change touches five backends instead of four.
- Headless jobs prove nothing about rendering. Visual correctness needs the separate golden-image jobs on real backends.
- A bug that only manifests with a real device is invisible to the headless jobs — which is why per-platform integration and golden-image jobs remain in the matrix.

### Implications for Future Work

- Every `RhiDevice` interface change updates the stub in the same commit. Reviewers enforce this.
- The dedicated server target builds against the stub rather than acquiring its own no-op device.
- New simulation systems must run headless. A system that requires a device is a design error, and the headless job will say so.
- Golden-image tests are the complement to this decision, not an alternative to it — see [Development §5.1](../development/REQUIREMENTS.md#51-layers).
