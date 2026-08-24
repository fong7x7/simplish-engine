# ADR-005: Deterministic lockstep for co-op

**Status:** Accepted
**Date:** 2026-08-22
**Scope:** Engine | Game

## Context

Co-op is 1–4 players. The simulation holds roughly 2,000 enemies and 20,000 projectiles, and the entities that matter most to the shared experience — the horde — are also the most numerous and the least individually important.

Replicating that state is the obvious approach and the wrong one here. Even aggressively culled and quantised, thousands of entity updates per tick is a bandwidth problem, and every one of those entities is a thing a player might be about to shoot, so relevance filtering does not save as much as it usually does.

[ADR-002](ADR-002-fixed-timestep-determinism.md) already commits to a deterministic simulation for replay and debugging reasons. That commitment makes a much cheaper option available.

## Decision

**Deterministic lockstep with input delay.** Peers exchange inputs, not state. Every peer simulates every tick identically from the same input set.

- Each peer broadcasts its input for tick `N` at tick `N - delay`. Input delay is configurable, defaulting to 2–3 ticks (33–50 ms), tuned against measured session RTT.
- A tick executes only once every peer's input for it has arrived. A late peer stalls the session — visibly, with feedback, rather than silently diverging.
- Per-tick state hashes are exchanged periodically. On divergence the session halts, captures both peers' recent tick traces, and reports.
- Joining happens at level boundaries only. A dropped peer's character persists under simplified control; rejoin restores control at the next boundary.
- No prediction, no rollback, no server reconciliation.

## Alternatives Considered

### Alternative A: Client-server with state replication

- **How it works:** An authoritative host simulates; clients receive entity state and render it, with local prediction for the player.
- **Pros:** The industry default. Tolerates non-determinism. Cheating is harder. Late join and drop are straightforward.
- **Cons:** Bandwidth scales with entity count, and the entity count here is the whole problem. Prediction and reconciliation are substantial engineering. It discards the determinism guarantee the project has already paid for.

### Alternative B: Rollback netcode (GGPO-style)

- **How it works:** Predict remote inputs, simulate ahead, roll back and resimulate when a prediction proves wrong.
- **Pros:** Hides latency almost entirely — why fighting games use it.
- **Cons:** Requires rolling back and resimulating the full simulation state, potentially several ticks. At 22,000 entities that means snapshotting and restoring megabytes per rollback, several times a second. Rollback works because fighting games have tiny state; this game has the opposite.

### Alternative C: Lockstep without input delay

- **How it works:** Same model, but a tick waits for inputs generated in that same tick.
- **Pros:** No added input latency in the ideal case.
- **Cons:** Every tick blocks on the slowest peer's round trip, so the session runs at the mercy of the worst connection and stutters constantly. Input delay is what converts a variable network cost into a fixed, tunable, and honest one.

## Design Principle References

- **Principle 1: Determinism Always** — this decision exists because the determinism guarantee is already in hand; it is the payoff, not an additional cost.
- **Principle 4: Simplicity Over Flexibility** — the entire netcode is an ordered input queue and a stall condition. There is no reconciliation layer to write, debug, or reason about.
- **Principle 2: Budgets Are Requirements** — bandwidth becomes proportional to player count, not entity count: a few hundred bytes per second per player regardless of what the horde is doing.

## Consequences

### Positive

- Bandwidth is trivial and independent of simulation scale.
- Netcode is small enough to reason about completely.
- Replay and netcode share one mechanism — a recorded session *is* a replay ([Engine §4.4](../engine/REQUIREMENTS.md#44-replay)).
- No client-server split in game code; there is one simulation path and it is the same one solo play uses.
- Desync is detected precisely, at the tick it happens, with the diverging subsystem named.

### Negative

- Input latency equals the configured delay, always — even in solo play if the same path is used, which is why solo bypasses the delay entirely.
- A slow peer degrades the session for everyone. There is no graceful partial degradation; this is inherent to lockstep and must be handled in the UI with honest feedback.
- Mid-level join is out of scope. Supporting it would require a full state transfer, which is the replication problem this decision avoids.
- Any determinism bug becomes a *multiplayer* bug — the session halts. This is correct behaviour, and it raises the stakes on the CI determinism gate considerably.
- Cheating is easier than under an authoritative server. Acceptable for co-op against AI; it would not be for competitive play.

### Implications for Future Work

- The CI cross-platform tick-hash gate is a release blocker, not a nicety ([Development §7](../development/REQUIREMENTS.md#7-ci-matrix)).
- Any new simulation system is a potential desync source and needs a determinism test before it ships.
- Distributor relays (Steam, Epic, console) plug in behind the transport interface; lockstep is unaware of which relay carries it ([Platform §4.3](../platform/REQUIREMENTS.md#43-distributor-services)).
- If competitive PvP ever enters scope, this decision must be revisited from scratch — lockstep's trust model does not survive it.
