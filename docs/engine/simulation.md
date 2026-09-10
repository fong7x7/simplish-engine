# Simulation — Tick, Pools, Hashing, Replay

**Parent document:** [Engine REQUIREMENTS](REQUIREMENTS.md) §4
**Package:** `src/engine/sim/` (`eng::sim`), plus `Pcg32` and `FixedStepClock` in `src/engine/core/`
**Governed by:** [ADR-002](../decisions/ADR-002-fixed-timestep-determinism.md), [ADR-004](../decisions/ADR-004-soa-pools-over-ecs.md), [ADR-005](../decisions/ADR-005-deterministic-lockstep-coop.md)
**Status:** Built and tested, and driven: `src/game/world` implements `SimulationSystems`, and the editor's playtest steps it ([Editor §1](../editor/REQUIREMENTS.md#current-state)). A standalone client is the next consumer.

The engine side of the deterministic simulation. It owns the tick order, entity handle bookkeeping, state hashing, and the replay format. The game owns everything that happens inside a tick, supplied through one interface.

---

## 1. The Shape

```
            FixedStepClock ──ticks──►┐            (live play: frame time → whole ticks)
  InputQueue ──TickInput──► Simulation::step ──TickResult──► ReplayRecorder
                                   │                              │
                     SimulationSystems (the game)           encodeReplay → bytes
                     playerControl … compaction                   │
                     hashState ──► TickHashBuilder          decodeReplay → verifyReplay
```

| Piece | Header | What it owns |
|---|---|---|
| `Simulation` | `engine/sim/simulation.h` | The tick counter and the phase order. Nothing else |
| `SimulationSystems` | `engine/sim/simulation-systems.h` | The game's side: one virtual per phase, plus `hashState` |
| `TickInput`, `PlayerInput` | `engine/sim/tick-input.h`, `player-input.h` | Everything a tick is a function of besides state: four players' quantised input |
| `InputQueue` | `engine/sim/input-queue.h` | §4.1 step 1. Takes a tick only once every player's input for it is in — the lockstep stall condition |
| `EntitySlots` | `engine/sim/entity-slots.h` | Generational handles, dense indices, deferred destruction, compaction |
| `StateHasher`, `TickHashBuilder`, `TickHash` | `engine/sim/state-hasher.h` … | The per-tick hash, one section per subsystem |
| `findDivergence` | `engine/sim/hash-divergence.h` | Which tick and which subsystem two runs disagree on |
| `ReplayRecorder`, `encodeReplay`, `decodeReplay`, `verifyReplay` | `engine/sim/replay-*.h` | Record, serialise, and check a run |
| `Pcg32` | `engine/core/pcg32.h` | The only RNG simulation code may use |
| `FixedStepClock` | `engine/core/fixed-step-clock.h` | Frame time in, whole 60 Hz ticks out, clamped at 4 a frame |

`Pcg32` and `FixedStepClock` live in `core` because [Engine §6](REQUIREMENTS.md#6-core-systems) puts RNG streams and the fixed clock there, and neither needs anything from `sim`.

---

## 2. The Tick

`Simulation::step(input)` builds a `TickContext` — the tick number and the input — then calls, in this order:

| §4.1 step | `SimulationSystems` method |
|---|---|
| 1. Drain the input queue | The `TickInput` handed to `step` |
| 2. Player controllers | `playerControl` |
| 3. Enemy AI and steering | `enemyAi` |
| 4. Weapon fire → projectile spawn | `weaponFire` |
| 5. Projectile integration and collision | `projectiles` |
| 6. Damage resolution and death | `damage` |
| 7. Spawn director | `director` |
| 8. Deferred destruction and compaction | `compaction` |
| 9. Tick hash | `hashState`, when hashing is on |

The order is the body of `Simulation::runPhases` — seven lines, in source, as [ADR-004](../decisions/ADR-004-soa-pools-over-ecs.md) asks. There is no scheduler and no registration. A game overrides the phases it has; the others are empty.

**Why one virtual per phase rather than `runPhase(enum)`.** An enum dispatch makes every game write a seven-case switch, which is a 16-line-rule violation waiting in every implementation, and hides the order behind an enum's declaration. Named methods put the order at one call site and let a game override only what it has.

**Time is the tick number.** `TickContext` carries no delta. A tick is always 1/60 s, so a duration is a tick count and a timer is a comparison against `context.tick`. Nothing in `src/engine/sim/` includes `<chrono>` or reads a clock.

**The caller decides when to step.** Live play feeds a `FixedStepClock`: `advance(elapsed_ns)` returns how many ticks to run, at most `MAX_TICKS_PER_FRAME`, reports the dropped remainder, and gives rendering an interpolation fraction. The clock keeps time in nanoseconds × 60, so 1/60 s is exactly 10⁹ units and an hour of 60 Hz frames is exactly 216,000 ticks. Playback steps through a replay's inputs; CI steps as fast as it can.

---

## 3. Entity Pools

A pool is `EntitySlots` plus one array per field, each `capacity()` long, indexed by dense index:

```cpp
struct RunnerPool {
  EntitySlots slots{2048};
  std::vector<Vec2> position = std::vector<Vec2>(2048);
  std::vector<Vec2> velocity = std::vector<Vec2>(2048);
};

// A system: linear over exactly the fields it reads.
for (uint32_t i = 0; i < pool.slots.size(); ++i) {
  pool.position[i] = pool.position[i] + pool.velocity[i];
}

// The compaction phase.
const auto moves = pool.slots.compact();
applySlotMoves(moves, pool.position);
applySlotMoves(moves, pool.velocity);
```

| Rule | Why |
|---|---|
| `EntityHandle{index, generation}` — `index` is a slot, stable for life; the dense position may move | A handle survives compaction; a stale one resolves to nothing instead of to the slot's next occupant |
| Generation 0 is never issued | A default-constructed handle is null |
| `destroy` only marks; indices stay valid until `compact` | Every system in a tick sees the same dense layout ([§4.2](REQUIREMENTS.md#42-entity-storage)) |
| `compact` fills holes from the highest dense index down, swapping in the last live entity | O(destroyed), and the result depends only on *what* was destroyed, never on the order destroys were requested in — which matters, because that order comes from whichever system noticed first |
| Capacity fixed at construction; a full pool's `spawn` returns nothing | No allocation during a tick. A pool sized too small is a loud authoring error, per ADR-004 |
| `hashInto` hashes live slots, every generation, and the free-list order | The free list decides the next handle `spawn` returns, so it is state |

Swap-and-pop means iteration order is not spawn order after a destroy. It is still deterministic — the same sequence of spawns and destroys always gives the same layout — and nothing in the contract requires spawn order.

ADR-004 anticipates a macro or generator for pool boilerplate "once three or four pools exist". `applySlotMoves` is the only helper so far; the rest waits for real pools in `src/game/`.

---

## 4. Determinism Contract, As Implemented

| Requirement ([§4.3](REQUIREMENTS.md#43-determinism-contract)) | Where it is enforced |
|---|---|
| Strict floating point | `SimplishCompilerOptions` sets `-ffp-contract=off -fno-fast-math` (`/fp:precise` on MSVC) for **every** target. Clang contracts `a*b+c` into FMA by default, arm64 always has FMA and x86_64 here does not, so without this the two architectures round differently — see [the write-up](../solutions/determinism/fma-contraction-diverges-arm64-x86.md) |
| PCG32 with named streams | `Pcg32(seed, stream)` matches the reference `pcg32_srandom_r` bit for bit (pinned by a test against the reference's published output). A game gives each system its own stream id |
| FX streams excluded from the hash | A cosmetic stream is simply never passed to `hashState`. `test_determinism` proves cosmetic draws leave every tick hash unchanged |
| No hash-map iteration, no unordered parallelism | Nothing in the package iterates an associative container or spawns a thread |
| No wall-clock reads | Nothing in the package includes `<chrono>`; `FixedStepClock` takes elapsed time as an argument |
| Per-tick 64-bit hash naming the diverging subsystem | `TickHashBuilder` sections; `findDivergence` returns the first differing section |

**Hashing values, not bytes of structs.** `StateHasher::add` and `addSpan` accept only types whose bytes are their value (`IS_HASHABLE_BITS`): integers, floats, and structs with no padding. A padded struct is a compile error, because its padding bytes are indeterminate and would make identical states hash differently. `Vec2` and `Vec3` opt in with a size check; a game's own float structs do the same.

**The hash algorithm is pinned.** `test_state_hasher` checks a golden value. Changing the hasher invalidates every checkpoint in every recorded replay, so it must be a deliberate change that bumps `REPLAY_FORMAT_VERSION`.

**Floats hash as bits.** `-0.0` and `+0.0` hash differently. That is correct — the contract is bit-identical state — and it is the first thing to check when two runs that "look the same" diverge.

---

## 5. Replay

A `Replay` is a `ReplayHeader` (level id, content hash, seed, player count), every tick's `TickInput`, and checkpoint `TickHash`es — [§4.4](REQUIREMENTS.md#44-replay)'s `{level id, content hash, seed, input stream}` plus what is needed to verify it.

**Recording.** `ReplayRecorder::record(input, result)` after each `step`, outside the tick. It keeps a checkpoint every `DEFAULT_CHECKPOINT_INTERVAL` (60) ticks and, in `finish`, the final tick. Ten minutes of input is reserved up front. Without tick hashing the replay still plays but cannot be verified.

**The format** (`replay-codec.h` documents it byte by byte): the magic `SRPL`, a u16 version, the header, then inputs as runs — a varint count of ticks identical to the previous one, then one changed tick written per player as a change mask and only the changed fields (axis deltas as zigzag varints, buttons as XOR). Held input costs nothing: ten minutes of it with four players encodes in under 64 bytes. Checkpoints keep their section hashes but not their names, which is why `findDivergence` takes names from the live run. Fixed-width integers are little-endian on every host.

**Decoding is hostile-input safe.** Replays arrive in crash reports and from other machines. `decodeReplay` never reads past the buffer, bounds every count (`MAX_REPLAY_TICKS` is six hours), rejects trailing bytes, and distinguishes `TRUNCATED` from `MALFORMED`, `BAD_MAGIC` and `UNSUPPORTED_VERSION`. Tests decode every strict prefix of a replay (all must be `TRUNCATED`) and every single-byte corruption of one, under the `asan` preset.

**Verification.** `verifyReplay(replay, simulation)` steps a freshly built simulation through the inputs and stops at the first checkpoint that disagrees, returning its tick and subsystem. That function is the replay-corpus CI job of [Development §5.1](../development/REQUIREMENTS.md#51-layers); the corpus itself waits on a game to record.

---

## 6. Testing a Simulation System

`src/engine/sim/test/support/spark-world.h` is a small `SimulationSystems` with the moving parts a real game has: an SoA pool, input-driven and RNG-driven spawns, float integration, deferred destruction, and a cosmetic RNG stream. `test_determinism.cpp` is the template for the determinism test every game system needs ([Development §5.2](../development/REQUIREMENTS.md#52-rules)):

- the same seed and inputs give the same combined hash on every tick, across repeated runs;
- one changed input diverges the run from exactly that tick;
- cosmetic RNG draws never change a hash;
- the scenario actually fills and drains its pool, so the test is not trivially passing.

`test_replay_verification.cpp` records a session, round-trips it through bytes, verifies it, and checks that a tampered input is reported at the next checkpoint with the right subsystem named.

---

## 7. Not Yet

| Gap | Waiting on |
|---|---|
| Per-phase tick timing for the playtest overlay ([Editor §7](../editor/REQUIREMENTS.md#7-playtest)) | The overlay. The playtest exists now; the timing must be measured by the caller — the simulation cannot read a clock — so it arrives as an observer when the overlay does |
| Replay corpus in CI | A game to record runs from |
| Content hash in the replay header | The content pipeline ([ADR-007](../decisions/ADR-007-json-authored-cpp-baked-content.md)); callers pass whatever they have |
| Pool growth "at explicit checkpoints" (§4.2) | A level-load path that sizes pools from metadata |
| Entry states for starting at a stage ([ADR-008](../decisions/ADR-008-level-scenario-hierarchy.md)) | ADR-008 being accepted; a replay then starts from an entry state instead of tick-0 initial state |
| Cross-platform hash agreement in CI | The CI matrix. The code is written for it (little-endian bytes, no FMA, reference PCG32), but it has only been run on macOS arm64 |
