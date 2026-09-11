# Simplish — Game Requirements

**Parent document:** [REQUIREMENTS.md](../../REQUIREMENTS.md)
**Version:** 0.1
**Status:** Draft
**Last Updated:** 2026-08-22

---

## 1. Overview

The game layer (`simplish-game`) is everything that makes Simplish a specific game rather than an engine: the player, the weapons, the enemies, the director that decides what comes next, and the rules of a run. It links `src/engine/` and is platform-agnostic — no SDL3, no graphics API, no distributor SDK. Platform-specific concerns reach it through engine interfaces.

**Current state:** `src/game/content`, `src/game/player` and `src/game/world` exist, and nothing else of the game does. Players spawn from a `GameSetup` as the character each picked from the `GameContent` character table — which gives them their move speed and health — move by their stick on the deterministic tick, and keep the aim they are given; the world composes them into the `SimulationSystems` the tick steps, and the editor's Play button runs it. Players collide with the level's solid props — an upright cylinder pushed out of axis-aligned boxes, sliding along them — but not with each other, and the ground has no height to follow. There is no weapon — the fire button is recorded, and heard by actors, but fires nothing. **Actors** exist in a first slice ([actors.md](actors.md)): any prop given a behavior in the editor is an enemy or NPC that perceives the players, runs its behavior's state machine ([ADR-009](../decisions/ADR-009-actor-behavior-state-machines.md)), plans paths round the props with A* (`engine/spatial`), steers and turns on the tick. They cannot yet attack, take damage, or be spawned by a director.

**The game:** an isometric horde shooter set in a collapsed modern world. One to four players hold hand-authored ground against escalating waves. The fantasy is *overwhelming volume, narrowly survived* — hundreds of enemies converging, thousands of projectiles in the air, and a build that turns that pressure into a body count.

---

## 2. Design Pillars

1. **Volume is the enemy.** No single attacker is a threat. The pressure comes from density, from the geometry of a room filling up, and from the moment a corridor stops being an escape route.
2. **Positioning over reflex.** The camera is fixed and everything is visible; the skill is in where you stand and what you kite into what, not in twitch reaction to what you couldn't see.
3. **Readable at all costs.** A screen with 2,000 enemies and 20,000 projectiles must still communicate what will kill you in the next second. Where fidelity and legibility conflict, legibility wins.
4. **Build expression.** Weapons and modifiers combine into distinct play patterns. A run is a series of decisions that change how you fight, not just numbers that go up.
5. **Co-op multiplies, not divides.** Four players face a harder, denser fight than one — the game scales up, and coordinated positioning is rewarded over four people soloing in parallel.

---

## 3. Player

A player plays as a **character**, picked before the run: a row of the project's characters data table ([project-format.md §8.1](../editor/project-format.md#81-what-the-editor-reads-today)) with a name, a model and the stats it plays by. The pick is part of the run's entry state ([ADR-008](../decisions/ADR-008-level-scenario-hierarchy.md)) — it is in the `GameSetup`, the stats are copied into the player at spawn, and the replay header records it — so it is simulation input, identical on every co-op peer. Built so far: move speed and health segments, and the editor's selector. A character's loadout — the weapons and consumables it starts with — joins the row when weapons exist; changing character mid-run, if the game wants it, is a level-boundary change to the next stage's entry state, never a write during a tick.

### 3.1 Movement

Twin-stick-style movement decoupled from aim: eight-way analogue movement on the left stick or WASD, aim on the right stick or mouse. Movement is continuous in world space, not tile-locked — the isometric grid governs terrain and pathing, not player position.

| Property | Requirement |
|---|---|
| Direction | Camera-relative: up on the stick or W moves up the screen, which under the isometric view is a diagonal across the grid. The turn into world axes happens where input is captured, so the simulation, the lockstep wire and replays carry world directions and none of them depends on the projection |
| Base speed | Tuned so crossing a standard room takes ~2 s; exact value data-driven |
| Acceleration | Short ramp (≤ 100 ms to full speed) — responsive, but with enough weight that direction changes cost something |
| Dodge | Short burst with i-frames on a cooldown; the primary defensive verb |
| Collision | Capsule swept against terrain and structures; enemies push but do not hard-block. Built so far: an upright cylinder resolved against props' boxes each tick (`engine/physics`); no terrain, no enemies, no sweep |
| Slowdown | Aiming and firing may apply a data-driven movement penalty per weapon |

### 3.2 Aiming and Firing

Free aim over a continuous 360°, independent of the eight facing directions the sprite renders. Mouse aim targets the cursor's world-plane projection; gamepad aim is stick-direction with a configurable deadzone and an optional soft assist that biases toward the nearest valid target without snapping.

Firing produces projectiles through the engine's projectile system. The game defines archetypes and fire patterns; the engine integrates and collides them.

### 3.3 Player State

| System | Requirement |
|---|---|
| Health | Discrete segments rather than a continuous bar — legible at a glance, and makes each hit's cost unambiguous |
| Armour / shields | Optional regenerating layer above health, data-driven per build |
| Death and revive | On death a player enters a downed state; a co-op teammate can revive within a window. Solo death ends the run |
| Ability slots | A small fixed number (target: 2) of cooldown-driven actives, filled by run rewards |
| Inventory | Two weapon slots plus consumables; no grid inventory, no weight management |

---

## 4. Weapons

Weapons are **data-driven definitions** in JSON, validated against a schema at load, hot-reloadable in development builds.

| Field group | Contents |
|---|---|
| Identity | ID, display name, tier, sprite/model refs, audio refs |
| Fire | Rate, burst shape, spread pattern, projectile archetype, projectile count, recoil impulse |
| Sustain | Magazine or heat model, reload or cooldown timing, overheat penalty |
| Feel | Screen shake, muzzle flash, camera kick, hit-stop on kill |
| Modifiers | Which modifier slots the weapon exposes and what they attach to |

Archetype families (each a distinct pressure-management answer, not a damage tier):

- **Sustained** — continuous output, positional commitment, rewards holding a line
- **Burst** — high per-trigger damage, dead time between, rewards timing
- **Area** — explosive or persistent zones, rewards funnelling and prediction
- **Piercing** — penetrates rows of enemies, rewards lining up the horde
- **Deployable** — turrets, mines, barriers; rewards reading where pressure will arrive

### 4.1 Modifiers

Modifiers attach to weapons and change behaviour, not just numbers: chain, ricochet, homing, split-on-impact, damage-over-time application, on-kill detonation. Stacking rules are explicit and validated — a modifier declares whether it stacks additively, multiplicatively, or is exclusive within its category. The projectile-count implications of every modifier combination are bounded at load, so no build can exceed the engine's projectile ceiling.

---

## 5. Enemies

### 5.1 Archetypes

| Archetype | Role | Behaviour |
|---|---|---|
| Swarmer | Baseline density | Direct pursuit via flow field, melee contact damage, cheap and numerous |
| Charger | Punishes standing still | Telegraphed windup, straight-line rush, overshoot and recover |
| Ranged | Punishes standing still differently | Maintains distance, fires telegraphed projectile volleys |
| Spitter | Zone denial | Lobs persistent hazard pools that reshape the usable floor |
| Shielded | Punishes undirected fire | Directional armour requiring flanking or piercing |
| Bloater | Punishes crowding | Detonates on death, damaging players and enemies alike |
| Elite | Wave punctuation | Scaled variant with a modifier and a distinct silhouette |
| Boss | Wave conclusion | Hand-authored phase script, arena-aware attacks |

### 5.2 Horde AI

Built so far ([actors.md](actors.md)): perception, behaviors as data state machines over closed sets of actions and conditions, a flow field per player built a budget of cells a tick and walked by every pursuer, per-actor A* with a per-tick expansion budget for everything else, separation against neighbours from a counting-sorted grid, a static-box broadphase, actors targeting actors, facing, and enemy archetypes as data — measured at 2,000 actors inside Engine §7's budget. The tiers are by distance to the nearest player alone, since screen presence is the client's and not the simulation's, and only perception runs at the reduced rate; per-objective fields, incremental recomputation and despawn culling wait on objectives and the director.

At 2,000 active enemies, per-entity pathfinding is not affordable. Movement runs on **shared flow fields**: the level's tile grid carries a field per player and per objective, recomputed incrementally as the world changes. Each enemy samples the field, applies local avoidance against its spatial-hash neighbours, and integrates. Individual pathfinding is reserved for the small number of enemies that need it — bosses, elites, and any archetype whose behaviour depends on a specific route.

Enemies are budgeted in tiers by distance and screen presence: on-screen enemies tick every frame; off-screen enemies tick at a reduced rate on a rotating schedule; enemies beyond the despawn radius are culled and refunded to the spawn budget. Tier assignment is deterministic — it is part of the simulation, not a rendering optimisation.

### 5.3 Damage and Death

Damage resolution is a single engine-side phase, so ordering is deterministic. Enemies have flat health, optional directional armour, and per-archetype hit reactions. Death spawns corpse decals (pooled, age-evicted) and any archetype-specific on-death effect. Corpses do not persist as simulated entities.

---

## 6. The Director

The director decides what the level throws at the players and when. It is deterministic — driven by seeded RNG streams and the level's authored script, never by wall-clock time or frame rate.

| Concern | Requirement |
|---|---|
| Wave structure | Waves are authored in the editor: composition, spawn points, timing, and trigger conditions |
| Pacing | Alternating pressure and relief; a wave's intensity curve is authored, and the director follows it |
| Spawn budget | A running cost ceiling on active enemies; composition is chosen to fill the budget, never exceed the engine's entity ceiling |
| Spawn placement | From authored spawn volumes, filtered by distance and line-of-sight to players — no spawning inside a player's field of view at close range |
| Difficulty scaling | Scales with player count and run progression along authored curves; scaling is transparent and inspectable in the editor |
| Adaptive response | Bounded: the director may shift within an authored band based on player performance, but never outside it. Authored intent wins |

---

## 7. Run Structure

> The roguelite-versus-campaign question is open — see [Project REQUIREMENTS §8](../../REQUIREMENTS.md#8-open-questions). This section specifies what is settled either way.

- Levels are **hand-authored** in the editor. Whatever the progression model, the spaces are designed, not generated.
- A run is a sequence of levels punctuated by reward choices that alter the build.
- Rewards are drawn from seeded pools; a run's full reward sequence is reproducible from its seed.
- Run state is serialised at level boundaries, so a run survives a crash or a quit.
- Meta-progression, if adopted, is stored separately from run state and never affects simulation determinism.

---

## 8. Co-op

Deterministic lockstep for 1–4 players, built on the engine's networking layer ([Engine §4.3](../engine/REQUIREMENTS.md#43-determinism-contract) and `src/engine/net/`).

| Concern | Requirement |
|---|---|
| Model | Lockstep: every peer simulates every tick from the same inputs. No prediction, no rollback, no authoritative reconciliation |
| Input delay | Configurable, defaulting to 2–3 ticks (33–50 ms), tuned against measured session RTT |
| Join | At level boundaries only. Mid-level join requires a full state transfer and is out of scope |
| Drop and rejoin | A dropped peer's character persists under simplified control; rejoin restores control at the next level boundary |
| Desync | Detected via per-tick state hashes. On divergence the session halts, captures both peers' recent tick traces, and reports rather than silently continuing |
| Scaling | Enemy density, health pools, and reward counts scale with player count along authored curves |
| Friendly fire | Off for direct damage; enabled for specific self-inflicted hazards (Bloater detonations, own deployables) so positioning still matters |
| Shared economy | Rewards are instanced per player; consumables and revives are shared |

---

## 9. Setting and Content Direction

Post-collapse modern: recognisable spaces gone wrong — parking structures, transit stations, flooded retail, industrial yards. Grounded weapons with an improvised edge. The palette is desaturated and wet, so that muzzle flashes, fires, and hazard hues carry the readability load described in [Engine §5.5](../engine/REQUIREMENTS.md#55-readability-rules).

Enemy silhouettes are the primary identification channel — an archetype must be identifiable from its outline alone at full horde density, before colour or animation is legible.

---

## 10. Non-Functional Requirements

| Requirement | Target |
|---|---|
| Content definitions | All weapons, modifiers, enemies, and waves are JSON, schema-validated at load, hot-reloadable in development builds |
| Balance iteration | A weapon or enemy tuning change requires no recompile |
| Determinism | Every game system respects the [engine determinism contract](../engine/REQUIREMENTS.md#43-determinism-contract); no game code reads wall-clock time or iterates a hash map during a tick |
| Simulation budget | Game systems fit within the engine's 6.0 ms tick budget at full horde load — see [Engine §7](../engine/REQUIREMENTS.md#7-non-functional-requirements) |
| Testability | Every game system is testable headless: the director, damage resolution, and modifier stacking have unit tests that run with no GPU |
| Input latency | ≤ 50 ms from input to on-screen response, solo; ≤ input delay + 50 ms in co-op |

---

## 11. Milestones

| Milestone | Scope |
|---|---|
| **M4 — Playable Slice** | Player movement, free aim, one weapon, two enemy archetypes (Swarmer, Ranged), flow-field pursuit, one hand-built level, one authored wave, win and lose states |
| **M5 — Combat Depth** | Full weapon archetype families; modifier system with stacking rules; remaining enemy archetypes; hit reactions and death effects; damage resolution complete |
| **M6 — Co-op** | Lockstep session integration, player-count scaling, revive, shared economy, drop and rejoin |
| **M8 — Game Complete** | Director with authored pacing and adaptive band; full content set; run structure and progression; rewards; save state; distributor integration |

---

*This document is a living spec. Engine capabilities it depends on live in [Engine REQUIREMENTS](../engine/REQUIREMENTS.md); authoring tools in [Editor REQUIREMENTS](../editor/REQUIREMENTS.md).*
