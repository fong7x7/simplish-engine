# Actors — Enemies and NPCs That Think

**Parent document:** [Game REQUIREMENTS](REQUIREMENTS.md) §5
**Packages:** `src/game/actors/` (`eng::game`), behaviors in `src/game/content/`, the world's wiring in `src/game/world/`
**Governed by:** [ADR-009](../decisions/ADR-009-actor-behavior-state-machines.md), [ADR-002](../decisions/ADR-002-fixed-timestep-determinism.md), [ADR-004](../decisions/ADR-004-soa-pools-over-ecs.md)
**Status:** Built and tested: perception in near and far tiers, the behavior machine, paths across the level, flow-field pursuit, separation by neighbour grid, steering, facing, patrol routes, actors targeting actors, and enemy archetypes — driven in the editor's playtest by any prop given a behavior, which can be paused, stepped a tick at a time, and watched through the navigation and AI overlays. Two thousand actors run inside Engine §7's 2.5 ms (§6). Attacks and damage, and the director that spawns archetypes, are not written — §8.

An **actor** is anything in the simulation that decides what to do: an enemy, an NPC, a companion. Its intelligence is a **behavior**. The word *agent* is not used for either, because in this repository it means the AI that drives the editor ([agent-api.md](../editor/agent-api.md)).

---

## 1. The Shape

```
GameSetup::actors ──► GameWorld ──► ActorPool (SoA, hashed as "actors")
                        │  brains_:     each behavior compiled once (ActorBrain)
                        │  grid_:       NavGrid from the setup's obstacles (engine/spatial)
                        │  broadphase_: the obstacles bucketed (engine/physics)
                        │  flow_:       a flow field per player (hashed as "flow")
                        │  ai_rng_:     PCG32 stream AI_RNG_STREAM (hashed as "ai_rng")
                        ▼
      enemyAi (§4.1 step 3) ── stepActors: flow fields → neighbours → perceive
                               → decide → intend → plan → steer → separate
                               → move → face
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
| `ActorRoute` | `game/actors/actor-route.h` | A patrol route: the points an actor on it walks between, in order. The world holds each distinct one once |
| `ActorWorkspace`, `ActorIntent`, `ActorCandidate` | `actor-workspace.h`, `actor-intent.h`, `actor-candidate.h` | Scratch a tick works in and throws away: the path finder, the neighbour grid, whom each actor might target, each actor's step |
| `ActorTargetKind` | `actor-target-kind.h` | Whether an actor's target is a player or another actor |
| `ActorFlowFields` | `actor-flow-fields.h` | Each player's flow field, and the one being rebuilt |
| `EnemyDefinition`, `findEnemy`, `makeEnemySpawn` | `game/content/enemy-*.h`, `game/actors/enemy-spawn.h` | An enemy archetype as data, and the `ActorSpawn` it becomes |
| `stepActors`, `spawnActor`, `compactActors`, `hashActors` | `game/actors/actor-system.h` | The tick's side |

---

## 2. A Tick

`GameWorld::enemyAi` runs after player movement, so actors react to where players are this tick. `stepActors` first spends the tick's budget on the flow fields and buckets every actor into the neighbour grid, then makes eight linear passes over the pool in dense order; each is a free function over the arrays it touches.

| Pass | What happens |
|---|---|
| **Flow fields** | One player's field is rebuilt at a time, 4,096 cells a tick (`ACTOR_FLOW_BUDGET_PER_TICK`), going round the players in slot order and skipping any whose field already leads to the cell they stand in. A finished field replaces that player's old one |
| **Neighbours** | Every actor's position, bucketed into the workspace's `NeighborGrid` for perception and separation |
| **Perceive** | An actor within 16 tiles of a player (`ACTOR_NEAR_TIER_TILES`) perceives every tick; one further off perceives every sixth tick, on the tick its slot picks, so the far ones share the work (Game §5.2's tiers, decided inside the simulation). Whom it might target is ranked — the target it has first, then the nearer, then players before actors, then dense index — and each is asked in turn until one is perceived: *seen* if within sight range, inside the view cone (a dot product against the cosine of half the view), and in line of sight with clearance 1; *heard* if a player holding fire within hearing range — walls do not stop sound. So the line walk is made once for most actors, not once per player. It remembers where it last perceived its target and forgets after `memory_ticks`, or when they leave the game. A neutral actor takes nobody |
| **Decide** | Interrupts, then the current state's exits, in authored order; the first whose condition holds is taken, once per tick. An exit to the state the actor is in is skipped. Entering a state clears its goal, path and movement flags |
| **Intend** | The state's action sets where to go and how close counts: pursue where the target was seen, stopping short; wander to a random spot near home; flee to a point away from the threat; walk to the next point of a patrol route; and so on (§3) |
| **Plan** | An actor chasing a player it perceives this tick walks down that player's flow field when it is complete and the actor is no wider than it was built for: a path of one waypoint four cells downhill, worked out afresh each tick. Within 6 tiles (`ACTOR_FLOW_DIRECT_TILES`) it walks straight at them when nothing is in the way, and only there asks for line of sight. Anyone else with a straight walk to its goal takes it; one without asks the `PathFinder` from the nearest open cell to the nearest open cell to its goal, within the tick's shared expansion budget, and keeps the smoothed waypoints. A path whose goal cell moved is replanned at most every `ACTOR_REPLAN_TICKS` (15) |
| **Steer** | Toward the next waypoint — or the goal once the path runs out — at the state's speed, never past the waypoint or the stopping distance. A charge steps along the facing and does not steer |
| **Separate** | Each actor is eased half the overlap out of every actor the neighbour grid finds near it, in the order it visits them, and wholly out of every player, who does not yield |
| **Move** | Integrate, then push out of the props the `BoxBroadphase` gathers within a tile past the actor's radius — the boxes the full list would have stopped it against — as a player is pushed out of them all. An actor that moved less than a quarter of its step is *blocked* |
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
| `pursue` | walks to where its target was seen — down their flow field when it is a player it perceives | `near_tiles`: stop within, default 0.9 — or touching, when the target is an actor, which would otherwise be shoved along ahead of it |
| `keep_distance` | backs off inside the band, closes in beyond it | `near_tiles`/`far_tiles`: 3 and 6 |
| `flee` | runs from where its target was seen | `far_tiles`: how far, default 8 |
| `follow` | keeps within reach of its target | `near_tiles`: default 2 |
| `search` | walks to where its target was last seen, and stands | — |
| `return_home` | walks back to where it spawned | — |
| `charge` | runs straight along its facing, at the state's speed | — |
| `patrol` | walks its route's points in order — round and round, or out and back when the state's `route` is `ping_pong` — and stands where it is with no route | — |

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

**Whom it targets** is a sense: `targets` is `players` (the default) or `opponents`. With `players`, hostile and friendly actors both take players as targets, and the behavior decides what that means — `pursue` or `follow`. With `opponents`, a hostile actor takes players and friendly actors, and a friendly one hostile actors: a guard that fights raiders. Other actors are found through the neighbour grid within sight range; they are seen, never heard, since nothing an actor does is loud yet. An actor's target is a handle and a kind (`ActorTargetKind`), because the two pools number their handles independently.

**Built-in presets** are always available, and a project's behavior with the same id replaces one: `idle`, `wander`, `guard` (watch, pursue within a 12-tile leash, search, return), `chase` (the swarmer: sees all round, pursues, searches), `skirmisher` (keeps distance, watching), `coward` (wanders, flees on sight), `follower` (keeps up, waits where it lost you), `charger` (pursue, wind up facing the target, rush 2.8× straight ahead, recover), `patrol` (walk the route at 70% speed, pursue on sight, search, go back to the route), and `defender` (a guard that targets opponents, seeing 240°).

A patrolling actor goes on to the next point once it stands within arrival distance of the one it is heading for, judged by where it is rather than by the `arrived` flag — so an actor pulled off its route by a pursuit takes it up again at the point it was heading for, not the first. Its place on the route (`route_leg`, and which way it is going on a `ping_pong` route) is actor state, and hashed.

A behavior that is not well formed — no states, or an index out of range — or an id nobody has, runs `idle`. `resolveBehavior` never fails, so a setup naming a behavior the content lacks is still a run every peer resolves the same way.

---

## 4. Facing

Facing is a unit vector, as a player's aim is; the simulation has no angles. A behavior's turn rate and view width are turned into a sine and cosine once, in `compileBrain`, through the deterministic `sinCosDegrees`. An actor's starting facing arrives in `ActorSpawn` as degrees and is turned into a vector the same way in `spawnActor` — degrees rather than a vector the editor computed, so peers cannot disagree about it.

In the editor a model is taken to face its own glTF +Z, which the Z-up conversion makes world −Y, so a prop at zero rotation faces −Y and its `yaw_degrees` is its Z rotation − 90° (`EDITOR_MODEL_FRONT_DEGREES`).

---

## 5. State and Hashing

Every `ActorPool` field is simulation state and is hashed, in section `actors`: body (position, facing, home, radius, height, brain, faction), mind (state, the tick it entered, target handle and kind, what it perceives, where and when it last perceived its target), movement (goal, whether the goal is kept, arrived, blocked, no path, and the whole `ActorPath`), and its patrol route (which route, the leg it is on, and which way it is walking it). `ActorPath` opts into `IS_HASHABLE_BITS` with a size check: it is floats and integers with no padding.

The AI stream's state is its own section, `ai_rng`. It is drawn from only in dense order — wander spots and `chance` conditions — so every peer draws the same numbers for the same actors.

The flow fields are the section `flow`, hashed by what determines them — each field's goal, clearance and whether it is complete, and the builder's goal and how many cells it has expanded — not cell by cell: a field is a pure function of the grid, its goal and its clearance.

`ActorIntent`, the candidate list, the neighbour grid and the path finder's scratch are recomputed each tick before they are read and are not hashed.

---

## 6. Budget

Engine §7 gives enemy AI and steering **2.5 ms for 2,000 actors**, and it is measured: `./scripts/perf-gate.sh` builds `relwithdebinfo` and runs the hidden `[perf]` case in `test_horde_budget.cpp` — 2,000 swarmers in a walled, pillared 64-tile arena closing on four circling players, seeing the whole room — and fails when the median tick of `enemyAi` is over budget. On an Apple M-series laptop the median is about **1.1 ms**, the 99th percentile about 1.3–1.8 ms.

| Cost | How it is held down | Before |
|---|---|---|
| Separation | the neighbour grid: each actor tests the few in the buckets it reaches | every pair — 90% of an 18.9 ms tick |
| Perception | ranked candidates, so one line walk rather than one per player; far actors every sixth tick | a line walk per player per actor |
| Planning | a pursuer walks its quarry's flow field: a few lookups a tick. A* for everything else, capped at 32,768 expansions a tick shared, 16,384 a search | A* per actor, a line walk to the goal each tick |
| Flow fields | 4,096 cells a tick, one field at a time, none for a player who has not changed cell | — |
| Collision | the broadphase's candidates within a tile | every prop box |
| Arithmetic | `Vec2` operators `constexpr` in the header, so they inline | a call per `+` |

The per-tick path and flow budgets are the hard ceilings: however many actors replan at once, planning costs at most that many expansions, and a request the budget cannot cover waits for the next tick. A field over the largest grid a level can have takes a few hundred ticks to rebuild; over a hand-authored level, a few.

---

## 7. In the Editor

An **enemy archetype** is an actor as data — model, health, body and behavior — in `content/data/enemies.data.json` ([project-format.md §8.3](../editor/project-format.md#83-the-enemies-table)), reported by `list_enemies`. `makeEnemySpawn` turns one into the `ActorSpawn` a prop given the same would have been, so a director spawning a horde and a designer placing a prop reach the game the same way; the horde benchmark spawns its swarmers from one. Nothing in the editor spawns them yet.

Any placed prop can run a behavior: its properties end with a **Behavior** row (None, the presets, the project's own) and, once it has one, **Faction** and **Route** rows. Saved as `behavior`, `faction` and `route` on the prop ([project-format.md §4.1](../editor/project-format.md#41-what-the-editor-writes-today)); reachable through `list_behaviors` and `set_behavior`, and every actor's position, facing, state and target through `get_playtest` ([capabilities.md](../editor/capabilities.md)).

A route is laid out with **waypoints** — general › tools › Waypoint in the browser, or `add_waypoint`. Each belongs to one of nine routes and has a place in it; dropping a waypoint while one is selected adds it to that route, after its last, so a route is laid out by dropping one after another. The viewport joins each route's waypoints in walking order, in the route's colour. `list_waypoints` reports every route's points and who patrols it.

A playtest can be paused (F6, Level › Pause Playtest) and stepped a tick at a time (F7, or `step_playtest` for any number of ticks). The View menu's **Navigation Overlay** shades what an actor cannot use — solid, too narrow for an actor a player's width, or walled off from every player start — and the status line names actors no path joins to a start (`get_navigation`; `find_path` plans a route between any two points). The **AI Overlay** draws, while playing, each actor's view cone, the path it is walking, a line to the target it sees, and its state's name. In a playtest the prop is drawn where its actor is, turned to its facing, playing its state's clip or its walk and idle clips; it is no longer a collision box for players. In the viewport an actor's footprint is outlined in its faction's colour with a tick the way it faces.

A rigged actor is a skinned mesh, and ADR-003's amendment allows **16 skinned instances a frame**: enough for the NPCs and set-piece enemies of a hand-authored level. A horde is sprites.

---

## 8. Not Yet

| Gap | Waiting on |
|---|---|
| Attacks and damage: `melee`, `fire`, `detonate` actions; `damaged`, `health_below` conditions | The damage phase, designed with weapons. An attack appends to an effects buffer the `damage` phase applies |
| Spawning archetypes, and culling actors beyond the despawn radius back into the spawn budget | The director (Game §6) |
| Flow fields per objective, and for actors wider than the default | Objectives; a wide actor plans with A* meanwhile |
| A pinned reference machine for the performance gate, and CI running it | The CI matrix of Development §7 |
| AI stand-ins for the multi-player preview and dropped co-op peers | Editor §7 and Game §8; they would feed `PlayerInput`, recorded like any input |
