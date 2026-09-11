# Actors — Enemies and NPCs That Think

**Parent document:** [Game REQUIREMENTS](REQUIREMENTS.md) §5
**Packages:** `src/game/actors/` (`eng::game`), behaviors in `src/game/content/`, the world's wiring in `src/game/world/`
**Governed by:** [ADR-009](../decisions/ADR-009-actor-behavior-state-machines.md), [ADR-002](../decisions/ADR-002-fixed-timestep-determinism.md), [ADR-004](../decisions/ADR-004-soa-pools-over-ecs.md)
**Status:** First slice built and tested: perception, the behavior machine, paths across the level, steering, facing — driven in the editor's playtest by any prop given a behavior. Attacks and damage, patrol routes, flow fields and actors targeting actors are not written — §8.

An **actor** is anything in the simulation that decides what to do: an enemy, an NPC, a companion. Its intelligence is a **behavior**. The word *agent* is not used for either, because in this repository it means the AI that drives the editor ([agent-api.md](../editor/agent-api.md)).

---

## 1. The Shape

```
GameSetup::actors ──► GameWorld ──► ActorPool (SoA, hashed as "actors")
                        │  brains_: each behavior compiled once (ActorBrain)
                        │  grid_:   NavGrid from the setup's obstacles (engine/spatial)
                        │  ai_rng_: PCG32 stream AI_RNG_STREAM (hashed as "ai_rng")
                        ▼
      enemyAi (§4.1 step 3) ── stepActors: perceive → decide → intend → plan
                                           → steer → separate → move → face
```

| Piece | Header | What it owns |
|---|---|---|
| `BehaviorDefinition` and its parts | `game/content/behavior-*.h` | A behavior as data: senses, movement, states, exits, interrupts |
| `builtInBehaviors`, `resolveBehavior` | `game/content/behavior-lookup.h` | The presets; finding a behavior by id, never failing |
| `Faction` | `game/content/faction.h` | Hostile, neutral, friendly |
| `ActorSpawn` | `game/actors/actor-spawn.h` | One actor a run starts with — part of `GameSetup` |
| `ActorBrain`, `compileBrain` | `game/actors/actor-brain.h` | A behavior with its rates and angles worked out per tick |
| `ActorPool` | `game/actors/actor-pool.h` | Every actor, one array per field |
| `ActorPath` | `game/actors/actor-path.h` | The route an actor is following: up to 16 smoothed waypoints |
| `ActorWorkspace`, `ActorIntent` | `actor-workspace.h`, `actor-intent.h` | Scratch a tick works in and throws away: the path finder, each actor's step |
| `stepActors`, `spawnActor`, `compactActors`, `hashActors` | `game/actors/actor-system.h` | The tick's side |

---

## 2. A Tick

`GameWorld::enemyAi` runs after player movement, so actors react to where players are this tick. `stepActors` makes eight linear passes over the pool in dense order; each is a free function over the arrays it touches.

| Pass | What happens |
|---|---|
| **Perceive** | For each player: *seen* if within sight range, inside the view cone (a dot product against the cosine of half the view), and in line of sight with clearance 1; *heard* if within hearing range and holding fire — walls do not stop sound. The actor keeps the target it has while it still perceives them, and otherwise takes the nearest perceived. It remembers where it last perceived them and forgets after `memory_ticks`. A neutral actor takes nobody |
| **Decide** | Interrupts, then the current state's exits, in authored order; the first whose condition holds is taken, once per tick. An exit to the state the actor is in is skipped. Entering a state clears its goal, path and movement flags |
| **Intend** | The state's action sets where to go and how close counts: pursue where the target was seen, stopping short; wander to a random spot near home; flee to a point away from the threat; and so on (§3) |
| **Plan** | An actor with a straight walk to its goal takes it. One without asks the `PathFinder` from the nearest open cell to the nearest open cell to its goal, within the tick's shared expansion budget, and keeps the smoothed waypoints. A path whose goal cell moved is replanned at most every `ACTOR_REPLAN_TICKS` (15) |
| **Steer** | Toward the next waypoint — or the goal once the path runs out — at the state's speed, never past the waypoint or the stopping distance. A charge steps along the facing and does not steer |
| **Separate** | Each actor is eased half the overlap out of every other actor and wholly out of every player, who does not yield |
| **Move** | Integrate, then push out of the level's props with `resolveCylinderAgainstBoxes`, as a player is. An actor that moved less than a quarter of its step is *blocked* |
| **Face** | Turned toward what the state faces, by at most the behavior's turn rate (`turnToward`) |

Actors never move players and never block them. Players are resolved before actors, so a player can walk into an actor and the actor steps aside on the same tick.

---

## 3. Behaviors

A behavior is a state machine written as data ([ADR-009](../decisions/ADR-009-actor-behavior-state-machines.md)); the JSON form is [project-format.md §8.2](../editor/project-format.md#82-the-behaviors-table).

| Action | The actor… | Distances |
|---|---|---|
| `idle` | stands, turning to watch a target it sees | — |
| `hold` | stands, facing as the state says | — |
| `wander` | walks to random spots near home, pausing at each | `far_tiles`: radius, default 4 |
| `pursue` | walks to where its target was seen | `near_tiles`: stop within, default 0.9 |
| `keep_distance` | backs off inside the band, closes in beyond it | `near_tiles`/`far_tiles`: 3 and 6 |
| `flee` | runs from where its target was seen | `far_tiles`: how far, default 8 |
| `follow` | keeps within reach of its target | `near_tiles`: default 2 |
| `search` | walks to where its target was last seen, and stands | — |
| `return_home` | walks back to where it spawned | — |
| `charge` | runs straight along its facing, at the state's speed | — |

| Condition | Holds when |
|---|---|
| `always` | always |
| `sees_target`, `hears_target` | the actor perceived its target that way this tick |
| `lost_target_for` *ticks* | it has not perceived its target for that long, or never has |
| `target_within`, `target_beyond` *tiles* | where its target was seen is that close, or further |
| `in_state_for` *ticks* | it has been in this state that long |
| `arrived` | it reached its goal last tick, or its action has none |
| `no_path` | its last plan found no way to its goal |
| `blocked` | it tried to move last tick and was stopped |
| `far_from_home` *tiles* | it is further than that from where it spawned |
| `chance` *permille* | a draw from the AI stream came in under it |

| Facing | Turns toward |
|---|---|
| `movement` | where it is going; standing, a target it sees |
| `target` | where its target was seen, whichever way it moves |
| `locked` | nothing — it keeps the heading it entered with |

**Built-in presets** are always available, and a project's behavior with the same id replaces one: `idle`, `wander`, `guard` (watch, pursue within a 12-tile leash, search, return), `chase` (the swarmer: sees all round, pursues, searches), `skirmisher` (keeps distance, watching), `coward` (wanders, flees on sight), `follower` (keeps up, waits where it lost you), and `charger` (pursue, wind up facing the target, rush 2.8× straight ahead, recover).

A behavior that is not well formed — no states, or an index out of range — or an id nobody has, runs `idle`. `resolveBehavior` never fails, so a setup naming a behavior the content lacks is still a run every peer resolves the same way.

---

## 4. Facing

Facing is a unit vector, as a player's aim is; the simulation has no angles. A behavior's turn rate and view width are turned into a sine and cosine once, in `compileBrain`, through the deterministic `sinCosDegrees`. An actor's starting facing arrives in `ActorSpawn` as degrees and is turned into a vector the same way in `spawnActor` — degrees rather than a vector the editor computed, so peers cannot disagree about it.

In the editor a model is taken to face its own glTF +Z, which the Z-up conversion makes world −Y, so a prop at zero rotation faces −Y and its `yaw_degrees` is its Z rotation − 90° (`EDITOR_MODEL_FRONT_DEGREES`).

---

## 5. State and Hashing

Every `ActorPool` field is simulation state and is hashed, in section `actors`: body (position, facing, home, radius, height, brain, faction), mind (state, the tick it entered, target handle, what it perceives, where and when it last perceived its target), and movement (goal, whether the goal is kept, arrived, blocked, no path, and the whole `ActorPath`). `ActorPath` opts into `IS_HASHABLE_BITS` with a size check: it is floats and integers with no padding.

The AI stream's state is its own section, `ai_rng`. It is drawn from only in dense order — wander spots and `chance` conditions — so every peer draws the same numbers for the same actors.

`ActorIntent` and the path finder's scratch are recomputed each tick before they are read and are not hashed.

---

## 6. Budget

Engine §7 gives enemy AI and steering **2.5 ms for 2,000 actors**. This slice is built for the handful a hand-authored level holds and is not measured against that yet:

| Cost | Now | At horde scale |
|---|---|---|
| Perception | every actor × every player, with a line walk each | staggered to every few ticks by slot |
| Planning | A* per actor, capped at 32,768 expansions a tick shared, 16,384 a search | flow fields, one per player, shared by every pursuer |
| Separation | every pair of actors | neighbours from a spatial hash |
| Collision | every actor × every prop box | a static-geometry broadphase |

The per-tick path budget is the one hard ceiling today: however many actors replan at once, planning costs at most that many expansions, and a request the budget cannot cover waits for the next tick.

---

## 7. In the Editor

Any placed prop can run a behavior: its properties end with a **Behavior** row (None, the presets, the project's own) and, once it has one, a **Faction** row. Saved as `behavior` and `faction` on the prop ([project-format.md §4.1](../editor/project-format.md#41-what-the-editor-writes-today)); reachable through `list_behaviors` and `set_behavior`, and every actor's position, facing, state and target through `get_playtest` ([capabilities.md](../editor/capabilities.md)). In a playtest the prop is drawn where its actor is, turned to its facing, playing its state's clip or its walk and idle clips; it is no longer a collision box for players. In the viewport an actor's footprint is outlined in its faction's colour with a tick the way it faces.

A rigged actor is a skinned mesh, and ADR-003's amendment allows **16 skinned instances a frame**: enough for the NPCs and set-piece enemies of a hand-authored level. A horde is sprites.

---

## 8. Not Yet

| Gap | Waiting on |
|---|---|
| Attacks and damage: `melee`, `fire`, `detonate` actions; `damaged`, `health_below` conditions | The damage phase, designed with weapons. An attack appends to an effects buffer the `damage` phase applies |
| Patrol routes | A route marker the editor places (`entity:waypoint`) and a `patrol` action |
| Actors targeting actors | The spatial hash |
| Flow-field pursuit, staggered perception, distance tiers | The horde (Game §5.2) and the performance gate's 2,000-actor benchmark |
| AI debug overlay in the playtest — paths, states, view cones — and pause and single-step | [Editor §7](../editor/REQUIREMENTS.md#7-playtest) |
| An `enemies` archetype table the director spawns from | The director (Game §6) |
| AI stand-ins for the multi-player preview and dropped co-op peers | Editor §7 and Game §8; they would feed `PlayerInput`, recorded like any input |
