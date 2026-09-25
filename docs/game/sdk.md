# The Game SDK — Writing a Project's Logic

**Status:** built — `src/game/sdk/` (the SDK) over `src/game/logic/` (the world it reaches).
**Read with:** [logic.md](logic.md), which covers how logic is built, played, checked and deployed, and the rules it keeps; [ADR-011](../decisions/ADR-011-project-game-logic-in-cpp.md).

A project's game logic is C++ in the project's `src/` folder. It is written against two things the engine provides:

| Layer | Package | What it is |
|---|---|---|
| **The world** | `game/logic` — `eng::game` | `GameLogicWorld`: everything the engine lets logic read and change, as an interface the host implements. Versioned: `GAME_LOGIC_API_VERSION` |
| **The SDK** | `game/sdk` — `eng::game::sdk` | What makes that pleasant: a `Game` base with event hooks, event and entity queries, the logic's own events and schedule, per-entity data, timers and phases, spawn patterns, dice |

Include one header — `<game/sdk/sdk.h>` — and both are there. The SDK calls the world through `GameLogicWorld` and nothing else, so its sources are compiled into the project's own library for a playtest (`cmake/SimplishGameLogic.cmake` does it) and linked with the engine for a deploy; a project links nothing itself.

---

## 1. A game in forty lines

```cpp
#include <game/sdk/sdk.h>

namespace sdk = eng::game::sdk;
using namespace eng::game;
using sdk::findActor;

namespace {

constexpr sdk::Every WAVES{sdk::seconds(20)};

class Arena final : public sdk::Game {
protected:
  void onTick(GameLogicWorld& world) override {
    if (WAVES.due(world.tick())) {
      sdk::spawnRing(world, {.centre = findActor(world, "gate")->position,
                             .radius = 7.0F,
                             .count = 6,
                             .enemy = "grunt",          // enemies.data.json
                             .actor = {.id = "wave"}});
    }
    if (kills_ >= 30) {
      world.endRun(RunOutcome::WON);
    }
  }

  void onActorDied(GameLogicWorld& world, const LogicEvent& death) override {
    ++kills_;
    if (death.id == "boss") {
      world.log("The boss is down");
    }
  }

  void onHash(GameLogicHash& hash) const override { hash.add(kills_); }

private:
  uint32_t kills_ = 0;
};

}  // namespace

SIMPLISH_GAME_LOGIC(Arena)
```

`SIMPLISH_GAME_LOGIC` is written once, in one source file. Every source is listed in `src/CMakeLists.txt`.

---

## 2. `sdk::Game` — the base

Derive from `sdk::Game` rather than `GameLogic`: it hands each event of the last tick to a hook before `onTick`, so a rule about deaths lives where deaths are.

| Hook | When |
|---|---|
| `onStart(world)` | Once, on tick 0 |
| `onActorSpawned(world, event)` | An actor the logic spawned joined the run |
| `onActorHurt(world, event)` | An actor was hurt, and lived |
| `onActorDied(world, event)` | An actor was killed — by anything. It is gone; `event.at` and `event.id` say where it fell and who it was |
| `onActorRemoved(world, event)` | The logic took an actor out without its dying |
| `onPlayerHurt(world, event)` | A player was hurt, and is still up |
| `onPlayerDowned(world, event)` | A player went down |
| `onPlayerRevived(world, event)` | A downed player is up again; `event.by` is the teammate who stood by them |
| `onPlayerOut(world, event)` | A downed player's window ran out, or nobody was left to revive them |
| `onActorStateEntered(world, event)` | An actor went into another state of its behavior — by its own exits or the logic's `setActorState`; `event.state` is the state's id |
| `onActorNoticed(world, event)` | An actor took someone new as its target; `event.other` is whom |
| `onActorAttacked(world, event)` | An actor struck, fired, spat or blew itself up; `event.other` is whom it had in mind |
| `onTick(world)` | Every tick, after the hooks above |
| `onRunEnded(world)` | Once, at the end of the tick the run ended on; `world.outcome()` says how. The tick after is never played, so writes do nothing — log the tally here |
| `onHash(hash)` | Fold every member a later tick decides anything by into the tick hash |

Events are the last tick's, in the order they happened; the world's `events()` gives the same list to a logic that is not a `Game`. Every hit that takes health is an event of its own, carrying the `amount` it took, its `cause` — `ATTACK`, `SHOT`, `BLAST`, `HAZARD` or `LOGIC` — and `by`, who is credited (§7):

```cpp
void onActorHurt(GameLogicWorld& world, const LogicEvent& hit) override {
  if (hit.cause == LogicDamageCause::BLAST) {
    blast_damage_ += hit.amount;
  }
}
```

### Asking the tick's events

Hooks hear events one at a time; queries ask of them all at once (`event-queries.h`), through an `EventFilter` — by `kind`, `target`, actor `id` or `id_prefix`, `by`, `cause` or `state`, each empty field letting every event through:

| Call | Gives |
|---|---|
| `findEvents(world, filter)` | The last tick's events passing it, in order |
| `countEvents(world, filter)`, `heard(world, filter)` | How many, and whether any |
| `totalAmount(world, filter)` | The health they took, summed |

```cpp
// Damage player 1 dealt last tick, and whether the boss took a blast.
const uint32_t dealt = sdk::totalAmount(world, {.by = player.target});
const bool rocked = sdk::heard(world, {.kind = LogicEventKind::ACTOR_HURT,
                                       .id = "boss",
                                       .cause = LogicDamageCause::BLAST});
```

### Events of your own

`sdk::Events<T>` is a queue of the logic's own events of type `T`, so one part of a logic can say what happened and every part that cares hears it, without the parts calling each other. `subscribe` a handler once — in the constructor or `onStart` — `emit` anywhere, and `dispatch(world)` where the logic's events are to be heard: each event in the order emitted, to each handler in the order subscribed, and one a handler emits in the same dispatch.

```cpp
struct WaveStarted { uint32_t wave; };
sdk::Events<WaveStarted> waves_;

void onStart(GameLogicWorld&) override {
  waves_.subscribe([this](GameLogicWorld& world, const WaveStarted& e) {
    spawnWave(world, e.wave);
  });
  waves_.subscribe([](GameLogicWorld& world, const WaveStarted&) {
    world.log("Here they come");
  });
}
void onTick(GameLogicWorld& world) override {
  if (WAVES.due(world.tick())) {
    waves_.emit({++wave_});
  }
  waves_.dispatch(world);
}
void onHash(GameLogicHash& hash) const override { waves_.hashInto(hash); }
```

Events not yet heard are state: `hashInto` folds them in, so `T` must be something `GameLogicHash::add` takes. Handlers are not.

### Later

`sdk::Schedule<T>` holds values for a tick to come — `at(tick, value)`, `after(world, ticks, value)` — and `runDue(world, run)` hands each whose tick has come to `run(world, value)`, by tick and then in the order added; a value added for now while running comes out in the same call. `cancel(drop)` drops those `drop(value)` says to, `nextTick()` says when the next is due, `hashInto` folds it all in. Where `Every` and `Cooldown` are checked each tick, a schedule is told once:

```cpp
struct Collapse { LogicTarget bridge; };
sdk::Schedule<Collapse> later_;

void onActorDied(GameLogicWorld& world, const LogicEvent& death) override {
  if (death.id == "lever") {
    later_.after(world, sdk::seconds(3), {findActor(world, "bridge")->target});
  }
}
void onTick(GameLogicWorld& world) override {
  later_.runDue(world, [](GameLogicWorld& w, const Collapse& c) {
    w.damage(c.bridge, 999);
  });
}
void onHash(GameLogicHash& hash) const override { later_.hashInto(hash); }
```

---

## 3. Entities

The world's players and actors are read as copies — `LogicPlayer`, `LogicActor` — each with a `target`, a stable handle to write through and to find it again by.

**Queries** (`actor-queries.h`, `player-queries.h`):

| Call | Gives |
|---|---|
| `findActor(world, "boss")` | The actor named so — the prop's id in the editor, or the id the logic spawned it with |
| `findActors(world, filter)`, `countActors(world, filter)` | Every actor passing an `ActorFilter` — by `faction`, `id_prefix`, living or dying too |
| `actorsWithin(world, at, radius, filter)` | Those within a radius, across the floor |
| `nearestActor(world, at, filter)` | The nearest passing one |
| `playersUp(world)`, `nearestPlayer(world, at)`, `playersCentre(world)` | Players who are up, the nearest, and the middle of them |
| `world.actorOf(target)`, `world.playerOf(target)` | The entity a kept target names now, or nothing when it is gone |

**Writes** (on `GameLogicWorld`, queued and applied in order when the tick's logic returns):

| Call | Does |
|---|---|
| `damage(target, n)`, `heal(target, n)` | Health, through the same path as a bite: grace, deaths, blasts |
| `moveTo(target, at)` | Teleports a player or actor; an actor forgets its path |
| `removeActor(target)` | Takes an actor out without a death or its blast |
| `setActorState(target, "enraged")` | Puts an actor into a state of its behavior, by id; false when it has none such |
| `setActorFaction(target, faction)` | Changes an actor's side |
| `spawnEnemy(archetype, at, id)`, `spawnActor(spawn)` | Adds an actor — see §6 |
| `endRun(outcome)` | Wins or loses the run |

**Your own data per entity** — `sdk::EntityData<T>` keeps a value per target, ordered by target so it iterates the same everywhere, and hashes itself:

```cpp
struct Marked { uint32_t hits = 0; };
sdk::EntityData<Marked> marked_;

void onActorHurt(GameLogicWorld& world, const LogicEvent& hit) override {
  if (++marked_[hit.target].hits == 3) {
    world.setActorState(hit.target, "flee");
  }
}
void onTick(GameLogicWorld& world) override {
  if (sdk::Every{sdk::seconds(5)}.due(world.tick())) {
    marked_.forgetGone(world);
  }
}
void onHash(GameLogicHash& hash) const override { marked_.hashInto(hash); }
```

`T` must be something `GameLogicHash::add` takes: integers, floats, enums, or a struct of them with no padding.

---

## 4. The level

| Call | Answers |
|---|---|
| `world.lineOfSight(from, to)` | Whether an actor of the default size could see across — no solid prop between |
| `world.walkable(at)` | Whether an actor could stand there |
| `world.obstacleCount()`, `world.obstacle(i)` | The level's solid props, as boxes |

Both spatial questions are asked of the navigation grid, which covers the level's player starts, actors and props with eight tiles to spare, and is fixed when the run starts; outside it the answer is false. **Players are not held to it**: a player can walk off the level's floor, and an actor spawned round them there has nowhere to stand — `spawnRing` skips such spots, and an actor put there anyway cannot plan a path. Spawn round fixed places of the level — where the players started, a named prop (`findActor(world, "gate")->position`) — rather than round players who may have wandered.

---

## 5. Time

A tick is 1/60 s and the tick number is the clock (`world.tick()`); nothing else may be.

| Tool | |
|---|---|
| `sdk::seconds(n)`, `sdk::minutes(n)` | Durations in ticks |
| `sdk::Every{interval, offset}` | `due(tick)` every `interval` ticks from `offset`. Stateless — nothing to hash |
| `sdk::Cooldown` | `ready(tick)`, `start(tick, length)`. A member: `hash.add(cooldown)` |
| `sdk::Phase<Stage>` | The logic's own stages over an enum of its own: `is`, `enter(stage, tick)`, `age(tick)`, `hashInto(hash)` |

---

## 6. Spawning

A run with logic has room for `GAME_LOGIC_ACTOR_CAPACITY` actors (2,048), the level's included; `world.actorRoom()` says how many more fit this tick. Spawns are applied after the tick's other writes and read from the next tick on, with an `ACTOR_SPAWNED` event.

- `world.spawnEnemy("grunt", at, "wave3")` — one of the project's enemy archetypes (`content/data/enemies.data.json`): health, body, behavior, side, model and death blast from the table.
- `world.spawnActor({.at = …, .behavior = "chase", .faction = …, .health = 3, .id = "pet", .model = "mesh:dog"})` — made to measure. `model` is an asset reference the playtest draws it with; empty is the stand-in.
- `sdk::spawnRing(world, {.centre, .radius, .count, .start_degrees, .enemy or .actor})` — evenly round a circle, each facing in, skipping spots inside props (`RingPlacement::ANYWHERE` to not). Returns how many were queued.

---

## 7. Combat

Players can only hurt things through the logic: the engine has projectiles, blasts and hazard pools, and the logic decides who fires what.

| Call | Does |
|---|---|
| `sdk::firing(world, player)` | Whether a player who is up holds fire this tick — trigger, left mouse, or `send_input`'s `fire`. Aim is `player.aim` |
| `sdk::fireWeapon(world, player, weapon, cooldown)` | Fires a `sdk::Weapon` — damage, speed, refire ticks, pellets, spread — when fire is held and the cooldown is ready. Keep a `Cooldown` per player in an `EntityData` and hash it |
| `world.fireShot({.from, .direction, .speed, .damage, .side})` | One projectile, flying from the next tick; it strikes props and bodies of the other side |
| `world.blast({.at, .radius, .damage})` | Hurts everyone within the radius, this tick; an actor it kills can go off in turn |
| `world.spawnHazard({.at, .radius, .damage, .ticks, .side})` | A pool on the floor biting the other side standing in it |

```cpp
constexpr sdk::Weapon SHOTGUN{.damage = 1, .refire_ticks = 40, .pellets = 5,
                              .spread_degrees = 30};
sdk::EntityData<sdk::Cooldown> triggers_;

void onTick(GameLogicWorld& world) override {
  for (const auto& player : sdk::playersUp(world)) {
    sdk::fireWeapon(world, player, SHOTGUN, triggers_[player.target]);
  }
}
void onHash(GameLogicHash& hash) const override { triggers_.hashInto(hash); }
```

Every shot fired, landed, and blast set off is cued like an actor's, so it flashes and is heard.

**Who did it.** Every hurt, death and downing names who is behind it in `event.by`: the player or actor that struck, fired or spilled the pool; for a blast's hits, whoever killed the one that went off — so a player who shoots an exploding actor is credited with what the explosion kills. `fireWeapon` credits the player firing; `fireShot`, `blast` and `spawnHazard` credit their `shooter` or `by`; `damage(target, n, by)` credits `by`. `sdk::playerBehind(world, event)` gives the credited player, when it was one:

```cpp
sdk::EntityData<uint32_t> kills_;   // by player

void onActorDied(GameLogicWorld& world, const LogicEvent& death) override {
  if (const auto killer = sdk::playerBehind(world, death)) {
    kills_[killer->target] += 1;
  }
}
```

---

## 8. Dice

`sdk::chance(world, permille)`, `sdk::between(world, low, high)` and `sdk::pick(world, span)` draw on the run's own logic stream — `world.random(bound)` underneath — so two machines draw the same numbers, and a draw added to the logic never shifts what actors roll.

---

## 9. Keeping it deterministic

Everything in [logic.md §4](logic.md#4-rules-the-logic-has-to-keep) holds. The SDK is built to make it easy: every container it offers is ordered, every timer is a tick number, every die is the world's, and `Game` gives `onHash` one obvious place to fold state in. What it cannot do is stop a member being left out of `onHash` — a divergence then shows only when two runs are compared.

---

## 10. Testing it

A logic test plays one of the project's levels with a fresh instance of the logic and checks what happens. Tests live in `src/tests/`, are listed under `TESTS` — `simplish_game_logic(SOURCES game-logic.cpp TESTS tests/game-logic-test.cpp)` — and are built into the library a playtest loads, never into a deployed game.

```cpp
SIMPLISH_LOGIC_TEST(the_rifle_kills_what_comes_from_ahead, "main") {
  test.hold(0, {.aim_x = 1.0F, .fire = true});   // player 1's controls
  test.run(2);
  const uint32_t before = sdk::countActors(test.world(), {.id_prefix = "wave"});
  test.run(sdk::seconds(4));
  test.expect(sdk::countActors(test.world(), {.id_prefix = "wave"}) < before,
              "a chaser fell to the rifle");
}
```

| On `test` | Does |
|---|---|
| `run(ticks)` | Plays that many ticks |
| `runUntil(done, max_ticks)` | Plays until `done(world)` holds, or `max_ticks` pass; whether it held |
| `hold(slot, {.move_x, .move_y, .aim_x, .aim_y, .fire})` | Holds a player's controls from the next tick until changed |
| `world()` | The world between ticks — every read of `GameLogicWorld`; writes through it do nothing |
| `logged(text)` | Whether the logic's `world.log()` has said something containing `text` |
| `expect(condition, what)` | Records a failure at this line when `condition` is false; the test goes on |

`SIMPLISH_LOGIC_TEST_WITH(name, "level", players)` plays with more than one player, up to four. The level is the saved file, except the open one, whose unsaved edits are played.

Every Build Game Logic runs every test after the determinism check, in the check's own process: a crash or a hang there fails the build rather than the editor. A failed `expect` fails the build and is reported as `path:line: error: test 'name': what` — a diagnostic in `get_build` like a compiler's — and `get_build`'s `tests` lists each test with its level, ticks, and failures.

---

## 11. Growing it

The SDK is engine code: a tool a second project would want belongs here, with a test in `src/game/sdk/test/` against the real `GameWorld`. What the world lets logic do is `GameLogicWorld`'s: a new read or write there is a new virtual function, implemented in `src/game/world/src/world-logic-view.cpp`, tested in `test_world_logic_api.cpp`, and a bump of `GAME_LOGIC_API_VERSION` so libraries built against the old table are refused rather than misread.
