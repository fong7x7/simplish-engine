# Simplish Project Format

**Status:** Specification — the manifest, the level file's props (their behaviors included), lights, player starts and waypoints, and the characters, behaviors and enemies data tables (§8.1–§8.3) are implemented; everything else is not
**Scope:** Editor | Engine | Build
**Governed by:** [ADR-007](../decisions/ADR-007-json-authored-cpp-baked-content.md)

What a project holds on disk, what each file means, and what it becomes when the game is built.

Everything here is authored as JSON and read back as JSON by the editor. Shipping builds compile generated C++ produced from these files; development builds load the JSON directly. Both produce the same in-memory tables — see ADR-007 for why the format carries that obligation.

---

## 1. What the Format Has to Do

| Requirement | Where it comes from | What it forces |
|---|---|---|
| Diff sensibly | [Editor §4.4](REQUIREMENTS.md#44-level-format) | Stable ordering everywhere; a change to one room touches one region of one file |
| Validate on write and load | Editor §4.4, §6 | Every file declares a schema id; the in-tree validator owns the rules |
| Feed a deterministic simulation | [ADR-002](../decisions/ADR-002-fixed-timestep-determinism.md) | No unordered containers in the serialised form; iteration order is file order |
| Survive being edited by hand | Editor §6 | Readable, commentable where JSON allows, no binary blobs inside JSON |
| Support hot-reload mid-playtest | [Engine §3](../engine/REQUIREMENTS.md) | Files are independently loadable; no global rebuild to apply one edit |
| Compile to C++ | ADR-007 | Every reference resolves at generation time; no dynamic shapes |

---

## 2. On-Disk Layout

```
my-project/
├── .simplish/
│   └── project.json          # manifest — exists today
├── assets/                   # source art: .obj, textures, audio
│   └── crate.obj
├── content/
│   ├── levels/
│   │   ├── main.level.json   # props and lights — exists today
│   │   └── roof.level.json   # a project holds as many as it is given
│   ├── encounters/
│   │   └── transit-station-waves.encounter.json
│   ├── scenarios/
│   │   └── act-one.scenario.json
│   ├── logic/
│   │   └── transit-station.logic.json
│   └── data/
│       ├── characters.data.json   # read today (§8.1)
│       ├── behaviors.data.json    # read today (§8.2)
│       ├── weapons.data.json
│       ├── enemies.data.json      # read today (§8.3)
│       └── projectiles.data.json
└── data/                     # editor-owned scratch: layouts, bookmarks
```

`assets/` is source material the asset pipeline consumes ([Editor §8](REQUIREMENTS.md#8-asset-pipeline)). `content/` is authored data this document specifies. The split matters: assets are imported and cached, content is generated and compiled.

`.simplish/project.json` and `content/levels/main.level.json` are the two files that exist today; `createProject` makes `.simplish/`, `data/`, `assets/` and `content/levels/` so a new project has this shape from the start.

**The manifest** holds what is true of the project rather than of any one level:

```json
{
  "name": "Transit Station",
  "engine_version": "0.1.0",
  "created_at": "2026-08-01T09:00:00Z",
  "last_opened_at": "2026-08-22T10:30:00Z",
  "default_workspace": "Level",
  "projection": "dimetric",
  "shading": "smooth"
}
```

`projection` is `"dimetric"` or `"isometric"`, and the View menu writes it when a projection is chosen. It belongs to the project rather than to the editor because tile art is authored against one of them ([ADR-003](../decisions/ADR-003-hybrid-iso-render-model.md#amendment-2026-09-08-projection-as-a-project-setting)). Absent or unrecognised, it reads as `"dimetric"`: every project written before the field existed was authored that way, and a project should open at a recoverable default rather than refuse to load.

`shading` is `"smooth"` or `"cel"`, and the View menu writes it too. It names one of the engine's mesh styles — continuous light, or banded light with an outline — rather than storing the style's numbers, because a project picks a look and the engine's presets define it. Absent or unrecognised, it reads as `"smooth"`, which is how every project written before the field existed has always been drawn.

---

## 3. Conventions Common to Every File

**Envelope.** Every content file is a JSON object with the same four keys before anything type-specific:

```json
{
  "schema": "simplish/level/1.0",
  "id": "transit_station",
  "name": "Transit Station",
  "content": { }
}
```

`schema` matches the id vocabulary the in-tree validator already uses (`SchemaDefinition::schema_id`, e.g. `"simplish/voxel_type/1.0"`). The trailing version is the format version, not the project's.

**Ids are stable, lowercase, and snake_case.** They are the name generated C++ uses, so they must be valid identifiers. An id is assigned once and never changes: renaming the display `name` is free, renaming an `id` is a refactor across every file that references it. The editor treats ids as immutable after creation and offers an explicit rename that rewrites references.

The editor already mints ids to this rule, ahead of the format that consumes them: assets are identified by path (`props/crate.obj` → `props_crate`), built-in shapes by name (`cube`), and each placed thing by what it instances and a number (`props_crate_01`). See `editor-entity-id.h`, and the properties panel, which shows the selected thing's qualified reference so it can be copied into a file by hand today.

**References are `kind:id` strings**, never file paths:

```json
"archetype": "enemy:runner",
"weapon": "weapon:auto_rifle",
"region": "region:platform_east"
```

Paths would break the moment a file moves; ids let the generator resolve across the whole project and fail the build on a dangling reference. The `kind` prefix is what lets the generator type-check a reference without loading the target first.

**Ordering is authored, not incidental.** Arrays serialise in the order the editor holds them, and that order is the order the runtime iterates. Nothing is sorted on save and nothing depends on a hash map. This is Principle 1 reaching into the file format: a reordered array is a different simulation, so the order has to be visible in the diff.

**Numbers are exact.** Integers where the value is discrete (tile indices, counts, weights in permille); floats only where the value is genuinely continuous, written with enough digits to round-trip.

---

## 4. Level Files

`content/levels/<id>.level.json` — the grid, and everything placed on it.

```json
{
  "schema": "simplish/level/1.0",
  "id": "transit_station",
  "name": "Transit Station",
  "content": {
    "bounds": { "min_x": 0, "min_y": 0, "width": 128, "height": 96 },
    "tile_palette": ["tile:void", "tile:concrete", "tile:grate", "tile:rubble"],
    "layers": {
      "terrain": { "encoding": "rle", "runs": [[0, 240], [1, 1580], [2, 64]] },
      "height":  { "encoding": "rle", "runs": [[0, 1884]] }
    },
    "props": [
      { "id": "crate_01", "asset": "mesh:crate", "at": [12.5, 30.0, 0.0],
        "yaw_steps": 0, "variant": 0 }
    ],
    "entities": [
      { "id": "spawn_north", "definition": "entity:spawn_point",
        "at": [40.0, 8.0, 0.0],
        "properties": { "faction": "hostile", "enabled": true } }
    ],
    "regions": [
      { "id": "platform_east", "kind": "spawn_volume",
        "rect": [64, 20, 24, 32] }
    ]
  }
}
```

**Tile layers are run-length encoded**, as [Editor §4.4](REQUIREMENTS.md#44-level-format) requires: `[palette_index, run_length]` pairs in row-major order. RLE is what makes the diff requirement achievable — repainting one room changes the runs covering that room and leaves the rest of the array untouched, where a flat array of 12,288 integers would reflow every line.

`tile_palette` indirects through ids so a palette edit does not rewrite the layer, and so the generated code gets a dense enum rather than sparse global ids.

**Props and entities are separate lists** because they behave differently: a prop is geometry with a transform and no per-instance state; an entity carries a definition id and a property block validated against that definition's schema ([Editor §4.2](REQUIREMENTS.md#42-props-and-entities)). `yaw_steps` is quarter turns, not radians — the projection has no yaw ([ADR-003](../decisions/ADR-003-hybrid-iso-render-model.md)), so props snap to four orientations and an integer keeps that exact.

`regions` are the named rectangles logic refers to: spawn volumes, trigger areas, objective zones. They are declared once here and referenced by id from encounter and logic files, so moving a region updates everything that uses it.

### 4.1 What the editor writes today

File > Save (`Ctrl`/`Cmd`+S) writes `content/levels/<id>.level.json` for the level the Level menu has open, and opening a project reads one back. `main` is where a new project starts and where an opened one goes back to when it has such a level; every other id is authored, through Level > New Level, which turns a typed name into an id the rules above allow. Its `name` is its id, because nothing in the editor shows or edits a level name yet — writing the project's name there instead would put the same name in every level file of a project holding several.

Three parts of §4 are written — props, lights, and two kinds of entity — and one of them differs from the shape above:

```json
{
  "schema": "simplish/level/1.0",
  "id": "main",
  "name": "Transit Station",
  "content": {
    "props": [
      { "id": "props_crate_01", "asset": "mesh:props_crate",
        "at": [3.0, 4.0, 0.0], "rotation": [0.0, 0.0, 45.0],
        "collides": true },
      { "id": "characters_knight_01", "asset": "mesh:characters_knight",
        "at": [5.0, 4.0, 0.0], "rotation": [0.0, 0.0, 0.0], "scale": 1.5,
        "collides": true, "animation": "walk",
        "behavior": "behavior:patrol", "faction": "hostile", "route": 1 }
    ],
    "lights": [
      { "id": "point_01", "kind": "point", "at": [1.0, 1.0, 3.0],
        "direction": [-0.35, -0.45, 0.82], "color": [1.0, 1.0, 1.0],
        "intensity": 1.0, "range": 8.0 }
    ],
    "entities": [
      { "id": "start_01", "definition": "entity:player_start",
        "at": [2.5, 3.5, 0.0], "properties": { "player": 1 } },
      { "id": "waypoint_01", "definition": "entity:waypoint",
        "at": [6.5, 4.5, 0.0], "properties": { "route": 1, "order": 1 } },
      { "id": "waypoint_02", "definition": "entity:waypoint",
        "at": [6.5, 9.5, 0.0], "properties": { "route": 1, "order": 2 } }
    ]
  }
}
```

**A prop carries three rotation angles, not `yaw_steps`.** The properties panel edits rotation X, Y and Z as free degrees and the agent API sets them the same way, so `yaw_steps` would round somebody's authored value away on the first save. The integer stays the right answer for the projection — which has no yaw — and the snap belongs with the tool that enforces it; when that tool arrives, a §10 migration converts a rotation to the steps it was rounding to. `variant` is absent because nothing produces one yet.

**A prop may carry a scale.** `scale` is one uniform size multiplier on the one-tile fit every dropped model gets, so `1` is the size it was dropped at. It is written only when it is not `1`, so a level saved before the key existed saves back unchanged, and a prop without it reads as `1`. Reading holds it to 0.125–8, the range the panel's slider covers — zero would leave nothing to pick or collide with, and a negative scale turns a model inside out. One number rather than three because the renderer's normals are only right under a uniform scale; a per-axis scale needs the mesh shaders to grow a normal matrix first.

**A prop says whether it collides.** `collides` is `true` when players cannot walk through it, which is what every prop dropped starts as, and `false` for the ones they can — grass, a rug, a decal. A prop written before the key existed has none and reads as `true`, so a level saved earlier is as solid as its props look. What collides is the prop's box, the one the viewport outlines; a collision shape authored per asset would go in the asset pipeline, not here.

**A rigged prop may name the clip it plays.** `animation` is the name of one of the model's animation clips, exactly as its glTF file names it, and is written only when a prop names one: a prop without it plays the model's first clip, which is what every rigged prop dropped does, and a static prop has no clips and never carries the key. A name the model does not have is kept rather than cleared, and plays the first clip too, so a clip renamed in the source file does not silently rewrite the level. The clip is presentation only — the simulation never reads a pose ([ADR-003 amendment](../decisions/ADR-003-hybrid-iso-render-model.md#amendment-2026-09-10-skinned-meshes-for-a-handful-of-characters)). A rigged model's asset reference is `mesh:` like any model on disk.

**A prop may run a behavior.** `behavior` names the intelligence the prop runs in a playtest, by reference — `behavior:guard` for a built-in behavior or a row of the behaviors table (§8.2) — and `faction` which side it is on: `"hostile"`, `"neutral"` or `"friendly"`. A prop with a behavior is an *actor* ([actors.md](../game/actors.md)): in a playtest it is spawned into the simulation at the middle of its tile, facing its Z rotation less a quarter turn, as wide as half the narrower side of its unturned footprint, and it is not a collision box for players. Both keys are written only when the prop has a behavior, so scenery carries neither and a level saved before they existed saves back unchanged. A bare id — `"guard"` — reads as `behavior:guard`; a reference to a behavior the project no longer has is kept as written and plays as `idle`; a faction the format does not know reads as `hostile`. An actor may also carry `route`, 1 to 9: the patrol route its behavior's `patrol` state walks — the waypoints of that route, below. It is written only when the prop has a behavior and a route; absent, or below 1, is none, and above 9 is held to 9. A route no waypoint belongs to any more is kept, and walks nowhere. This is the one place a prop carries per-instance game state rather than only geometry, which §4's split between props and entities anticipated for entities: the prop already holds the model, transform and clip an actor needs, and an entity for it would duplicate them.

**Lights are the array §4 does not list**, because the editor's lighting arrived before this document did ([Editor §1](REQUIREMENTS.md#1-overview)). A light is one record for both kinds — `kind` is `"directional"` or `"point"` — and a field the kind ignores is written anyway rather than left as a hole. An unrecognised `kind` reads as directional, on the same rule an unrecognised `projection` reads as dimetric.

**The editor knows two entity definitions: the player start and the waypoint.** It is dragged from the browser's general › tools section and marks where a player spawns: `player` is which of the session's four players, 1 to 4, and more than one start may name the same player — which of them the game uses is the game's decision. It has no facing, because the camera never rotates and players aim freely. An optional `character` property names who the player who spawns there plays as unless they pick someone else — a row of the characters table (§8.1), by reference: `"character": "character:scout"`. It is written only when there is one; a bare id reads as `character:<id>`, and a reference to a character the table no longer has is kept as written rather than dropped — the start is still a start without it, and its player picks as though it named none. Its id is numbered from `start` rather than from its player (`start_01`), since the player can be changed and an id cannot; another file references it as `player_start:start_01`. Reading holds `player` to 1–4 and gives a start with no id one, and an entity whose `definition` is anything else is dropped and counted, as a prop naming a missing asset is — so a hand-written `entity:spawn_point` does not survive a save until the editor has a definition for it.

**A waypoint is a point of a patrol route.** Dragged from general › tools like the player start; `route` is which of a level's nine routes it belongs to, 1 to 9, and `order` its place in it, 1 to 99. An actor walks a route's waypoints by `order`, and in file order where two share one, so a hand-written route need not be numbered without gaps. Reading holds both to their ranges — a waypoint with neither is the first place of route 1 — and gives one with no id one, numbered from `waypoint` (`waypoint_01`); another file references it as `waypoint:waypoint_01`. A waypoint's height is kept but walks nowhere: patrols walk the floor. Routes are the level's, not an actor's, so two guards can share one and moving a waypoint moves every patrol that walks it.

Everything else in §4 — bounds, the tile palette, the RLE layers, the other entity definitions and regions — is unwritten, and a file this editor reads is not required to carry it. What it does read is strict about one thing: a `schema` that is not `simplish/level/1.0` is refused outright rather than partly read, per §10.

**A prop names its asset by reference, never by index.** `mesh:props_crate` for a model on disk, `shape:cube` for a built-in shape. The index a session holds an asset at is renumbered by any rescan, so a level saved with indices would decay the moment a file was added beside it; a reference is resolved against the scan when the level loads. A prop whose asset the project no longer holds is dropped on load and counted in the log — one deleted `.obj` costs that prop and nothing else. A prop with no `id` is given one, so nothing in a level is unnameable even after a hand edit.

---

## 5. Encounter Files

`content/encounters/<id>.encounter.json` — what the director executes ([Editor §5](REQUIREMENTS.md#5-encounter-and-wave-authoring)).

```json
{
  "schema": "simplish/encounter/1.0",
  "id": "transit_station_waves",
  "name": "Transit Station — Waves",
  "content": {
    "level": "level:transit_station",
    "waves": [
      {
        "id": "wave_1",
        "start": { "on": "encounter_start", "delay_ticks": 180 },
        "composition": [
          { "archetype": "enemy:runner", "count": 24, "weight_permille": 700,
            "regions": ["region:platform_east"] },
          { "archetype": "enemy:brute", "count": 2, "weight_permille": 300,
            "regions": ["region:platform_east"] }
        ],
        "cadence_ticks": 30,
        "intensity_curve": [[0, 0], [600, 700], [1800, 1000], [2400, 200]],
        "player_scaling": { "2": 1400, "3": 1750, "4": 2000 },
        "adaptive_band_permille": 150
      }
    ]
  }
}
```

**Time is in ticks, never seconds.** The simulation is fixed-timestep; a wave that starts "after 3 seconds" is a wave whose start depends on frame rate. Ticks make the authored value the simulated value.

**Ratios are permille integers**, not floats. A designer-drawn intensity curve stored as `0.7` invites a float comparison in the director; `700` does not. The curve is a list of `[tick, intensity_permille]` points the director interpolates.

`player_scaling` is keyed by player count because that is how it is authored and read; the generator turns it into a fixed four-element array, so no map lookup survives into the simulation.

---

## 6. Scenario Files

`content/scenarios/<id>.scenario.json` — the sequence a run moves through, above the level ([Game §7](../game/REQUIREMENTS.md#7-run-structure)).

```json
{
  "schema": "simplish/scenario/1.0",
  "id": "act_one",
  "name": "Act One",
  "content": {
    "stages": [
      { "id": "arrival", "level": "level:transit_station",
        "encounter": "encounter:transit_station_waves",
        "objectives": ["objective:reach_platform", "objective:survive_waves"],
        "on_complete": { "next": "stage:descent" } }
    ],
    "objectives": [
      { "id": "reach_platform", "kind": "reach_region",
        "region": "region:platform_east", "optional": false },
      { "id": "survive_waves", "kind": "survive_encounter",
        "encounter": "encounter:transit_station_waves", "optional": false }
    ]
  }
}
```

Objectives are declared with a `kind` from a closed set, not as free-form script. Each kind maps to a generated struct and a handler the runtime already implements — which is what keeps the scenario file data rather than a program.

---

## 7. Logic and Expressions

`content/logic/<id>.logic.json` — triggers: when something happens, if some conditions hold, do these things.

```json
{
  "schema": "simplish/logic/1.0",
  "id": "transit_station_logic",
  "name": "Transit Station — Logic",
  "content": {
    "triggers": [
      {
        "id": "open_gate_on_clear",
        "when": { "event": "wave_cleared", "wave": "wave:wave_1" },
        "conditions": [
          "player.count >= 1",
          "level.flag_gate_open == false"
        ],
        "actions": [
          { "do": "set_flag", "flag": "gate_open", "value": true },
          { "do": "open_door", "prop": "prop:gate_north" },
          { "do": "start_wave", "wave": "wave:wave_2" }
        ],
        "once": true
      }
    ]
  }
}
```

**The shape is fixed: event, conditions, actions.** No loops, no branching beyond the condition list, no user-defined functions. That is what [Principle 4](../development/design-principles.md) asks for, and it is what makes the generated form a plain function.

**Conditions are expressions**, and this is the part of the format with the sharpest constraint. On the JSON path they run through `ExpressionEvaluator` — already in-tree, already sandboxed, read-only, bounded. On the generated path the same string is emitted as a C++ expression:

```cpp
// generated from "player.count >= 1 && level.flag_gate_open == false"
if (ctx.player.count >= 1 && ctx.level.flag_gate_open == false) { ... }
```

The two must agree exactly, on every platform. That rules constructs out of the grammar:

| Allowed | Why |
|---|---|
| Integer and boolean comparison, `&&`, `\|\|`, `!` | Identical semantics interpreted and compiled |
| Whitelisted property reads (`player.count`, `level.flag_*`) | Resolvable at generation time; a typo is a build error |
| Integer arithmetic `+ - *` | Exact in both paths |
| **Not allowed:** floating-point comparison | `0.1 + 0.2 == 0.3` differs by evaluation order and platform; a desync waiting to happen |
| **Not allowed:** division | Rounding and divide-by-zero differ between the interpreter and the compiler |
| **Not allowed:** string operations | No bounded cost, no clean C++ mapping |

A golden test pins this: a corpus of expressions evaluated both ways, asserted equal. When someone widens the grammar later, that test is what tells them whether the widening is safe.

**Actions come from a closed set**, each with a schema. Adding an action means adding a handler to the runtime and a case to the generator — deliberately, so the set of things content can do to the simulation stays enumerable and reviewable.

---

## 8. Data Tables

`content/data/<name>.data.json` — the tables [Editor §6](REQUIREMENTS.md#6-data-editing) edits and the runtime validates.

```json
{
  "schema": "simplish/data_table/1.0",
  "id": "weapons",
  "name": "Weapons",
  "content": {
    "entry_schema": "simplish/weapon/1.0",
    "entries": [
      { "id": "auto_rifle", "name": "Auto Rifle",
        "damage": 12, "fire_interval_ticks": 6, "magazine": 30,
        "projectile": "projectile:rifle_round" }
    ]
  }
}
```

`entry_schema` names the schema every entry validates against, so the editor renders fields by type and the generator knows the struct to emit.

### 8.1 What the editor reads today

Three tables. This one is the characters a player can play as: `content/data/characters.data.json`, entry schema `simplish/character/1.0`. The others are the behaviors props run (§8.2) and the enemy archetypes the director will spawn (§8.3).

```json
{
  "schema": "simplish/data_table/1.0",
  "id": "characters",
  "name": "Characters",
  "content": {
    "entry_schema": "simplish/character/1.0",
    "entries": [
      { "id": "scout", "name": "Scout", "model": "mesh:characters_scout",
        "move_speed": 7.5, "health": 3 },
      { "id": "tank", "name": "Tank", "model": "shape:cylinder",
        "move_speed": 3.5, "health": 9 }
    ]
  }
}
```

| Field | Means | Absent |
|---|---|---|
| `id` | What a player start (`character:scout`), a setup and a replay name it by. Lowercase, digits and underscores | The row is skipped |
| `name` | What the selector and the Character row call it | The id |
| `model` | The asset it is drawn as, by reference — `mesh:…` or `shape:…` | The stand-in cylinder |
| `move_speed` | Tiles a second at full stick, 0 to 20 | 5, the default character's |
| `health` | Health segments it starts with, 1 to 99 | 5 |

Reading is forgiving, as the level reader is, because the file is written by hand: a row with no usable id or a repeated one is skipped, a stat out of range is held to it, and each is logged and reported by `list_characters` rather than refusing the file. A file that is not a characters table gives no characters. The editor writes nothing here: the table is authored by hand until the data-editing panel ([Editor §6](REQUIREMENTS.md#6-data-editing)) exists. At run time it becomes the `game::GameContent` the simulation is built with — the one representation both loaders of [ADR-007](../decisions/ADR-007-json-authored-cpp-baked-content.md) fill; a loadout joins each row when weapons exist.

### 8.2 The behaviors table

`content/data/behaviors.data.json`, entry schema `simplish/behavior/1.0` — the intelligence props run in a playtest ([ADR-009](../decisions/ADR-009-actor-behavior-state-machines.md), [actors.md](../game/actors.md)). Optional: every project has the game's built-in behaviors — `idle`, `wander`, `guard`, `chase`, `skirmisher`, `coward`, `follower`, `charger`, `patrol` — and a row here with one of their ids replaces it.

```json
{
  "schema": "simplish/data_table/1.0",
  "id": "behaviors",
  "name": "Behaviors",
  "content": {
    "entry_schema": "simplish/behavior/1.0",
    "entries": [
      { "id": "sentry", "name": "Sentry",
        "senses": { "sight_range": 12, "view_degrees": 120,
                    "hearing_range": 6, "memory_ticks": 300 },
        "movement": { "speed": 3.0, "turn_degrees_per_second": 270 },
        "initial": "watch",
        "interrupts": [ { "when": "far_from_home", "tiles": 15, "to": "go_home" } ],
        "states": [
          { "id": "watch", "do": "idle",
            "exits": [ { "when": "sees_target", "to": "pursue" },
                       { "when": "hears_target", "to": "search" } ] },
          { "id": "pursue", "do": "pursue", "stop_within": 1.0,
            "exits": [ { "when": "lost_target_for", "ticks": 120, "to": "search" } ] },
          { "id": "search", "do": "search", "face": "target",
            "exits": [ { "when": "sees_target", "to": "pursue" },
                       { "when": "in_state_for", "ticks": 300, "to": "go_home" } ] },
          { "id": "go_home", "do": "return_home",
            "exits": [ { "when": "arrived", "to": "watch" } ] }
        ] }
    ]
  }
}
```

| Field | Means | Absent |
|---|---|---|
| `id` | What a prop names it by: `behavior:sentry` | The row is skipped |
| `name` | What the Behavior row calls it | The id |
| `senses` | `sight_range` and `hearing_range` in tiles, `view_degrees` (360 sees all round), `memory_ticks`, and `targets` — whom it takes as a target: `players`, or `opponents` (for a hostile actor the players and friendly actors, for a friendly one hostile actors; an unknown word is `players`) | 10, 6, 180, 300, `players` |
| `movement` | `speed` in tiles a second, as a character's; `turn_degrees_per_second` | 3.5, 360 |
| `initial` | The state an actor starts in | The first state |
| `interrupts` | Exits tested before the current state's own, in every state | None |
| `states` | At least one, at most 32 | The row is skipped |

A **state** has an `id` unique within its row, an action `do`, a `face` (`movement`, `target`, `locked`; absent is `movement`), a `speed_permille` (1000 is the behavior's speed), an optional `clip` — the animation a rigged actor plays in it, presentation only — and `exits`. Its distances are named by its action: `stop_within` for `pursue` and `follow`, `min` and `max` for `keep_distance`, `radius` for `wander`, `distance` for `flee`. A `patrol` state takes a `route` instead: `"loop"` (the default) walks from its last waypoint back to its first, and `"ping_pong"` turns round at either end; which route an actor walks is the prop's, not the behavior's (§4.1). The actions are `idle`, `hold`, `wander`, `pursue`, `keep_distance`, `flee`, `follow`, `search`, `return_home`, `charge`, `patrol`.

An **exit** has a `when` and a `to` — a state of the same row, by id — and the one number its condition reads: `tiles` for `target_within`, `target_beyond` and `far_from_home`; `ticks` for `lost_target_for` and `in_state_for`; `permille` for `chance`. The other conditions — `always`, `sees_target`, `hears_target`, `arrived`, `no_path`, `blocked` — read none. Exits are tested in the order written; the first that holds is taken.

Durations are ticks and chances permille, as in encounter files; speeds are tiles a second, as in the characters table.

Reading is forgiving, as the characters reader is, but never so forgiving that a behavior it keeps could index past its own states: a row or state with no usable or a repeated id is skipped; an exit whose `when` is no condition or whose `to` names no state is skipped; an unknown `do` is `idle` and an unknown `face` is `movement`; a number that is not one takes its default and one out of range is held to it; a row left with no states is skipped. Each is logged and reported by `list_behaviors`. The editor writes nothing here: the table is authored by hand until the data-editing panel ([Editor §6](REQUIREMENTS.md#6-data-editing)) exists. At run time it becomes `game::GameContent::behaviors`, beside the characters.

### 8.3 The enemies table

`content/data/enemies.data.json`, entry schema `simplish/enemy/1.0` — the enemy archetypes of Game §5.1, as the director will spawn them ([actors.md](../game/actors.md)). Optional, and nothing spawns from it yet: the director reads it when it exists. Until then the editor reads and reports it, and a playtest carries it in its content.

```json
{
  "schema": "simplish/data_table/1.0",
  "id": "enemies",
  "content": {
    "entry_schema": "simplish/enemy/1.0",
    "entries": [
      { "id": "swarmer", "name": "Swarmer", "model": "mesh:enemies_swarmer",
        "health": 1, "radius": 0.3, "height": 1.2, "behavior": "chase" },
      { "id": "bloater", "name": "Bloater", "model": "mesh:enemies_bloater",
        "health": 4, "radius": 0.6, "behavior": "behavior:chase",
        "faction": "hostile" }
    ]
  }
}
```

| Key | Means | Absent |
|---|---|---|
| `id` | Stable identifier: lowercase, digits and underscores, unique in the table | The row is skipped |
| `name` | What the editor calls it | Its id |
| `model` | The asset it is drawn as, by reference | The stand-in |
| `health` | Health segments, 1–999. Kept, and used by nothing until actors take damage | 1 |
| `radius`, `height` | Its body, in tiles: 0.05–2 and 0.1–8 | 0.3, 1.5 — an actor's default |
| `behavior` | A behavior by id or `behavior:` reference: a built-in or a row of §8.2 | `idle`, and said |
| `faction` | `hostile`, `neutral` or `friendly` | `hostile` |

Reading is forgiving in the way §8.1 is: a row with no usable or a repeated id is skipped, a number that is not one takes its default and one out of range is held to it, and an unknown faction is hostile. A behavior nobody defines is kept as written — the behaviors are read separately and may be fixed — and `list_enemies` says whether each archetype's behavior resolves. At run time the table becomes `game::GameContent::enemies`; `makeEnemySpawn` turns a row into the same `ActorSpawn` a prop with that model, behavior and faction would have become.

---

## 9. What It Becomes

The generator reads a validated project and writes C++ into the build directory. Nothing generated is committed.

```
build/<preset>/generated/content/
├── content-ids.h          # enums: every id in the project, densely numbered
├── content-tables.h       # struct declarations and the accessor
├── content-tables.cpp     # the constexpr tables
├── content-logic.cpp      # one function per trigger
└── level-<id>-tiles.cpp   # bulk tile payload, one TU per level
```

An id enum, so a reference is an integer and a switch over it is dense:

```cpp
enum class EnemyArchetypeId : uint16_t { RUNNER = 0, BRUTE = 1, COUNT = 2 };
```

A table, as data the compiler can fold:

```cpp
inline constexpr WeaponDef WEAPONS[] = {
    {.id = WeaponId::AUTO_RIFLE, .damage = 12, .fire_interval_ticks = 6,
     .magazine = 30, .projectile = ProjectileId::RIFLE_ROUND},
};
```

A trigger, as a function:

```cpp
void trigger_open_gate_on_clear(const LogicContext& ctx, LogicEffects& out) {
  if (!(ctx.player.count >= 1 && ctx.level.flag_gate_open == false)) return;
  out.setFlag(LevelFlag::GATE_OPEN, true);
  out.openDoor(PropId::GATE_NORTH);
  out.startWave(WaveId::WAVE_2);
}
```

Actions do not mutate the simulation directly — they append to an effects buffer the tick applies at a defined phase. A trigger that wrote straight into entity state would make its effect depend on when it ran within the tick, which is exactly the kind of ordering dependency [ADR-002](../decisions/ADR-002-fixed-timestep-determinism.md) exists to prevent.

**Bulk tile payloads live in their own translation unit per level**, as a `constexpr` byte array rather than an initializer list of structs. Editing one level then recompiles one object file, and the compiler's worst case is isolated where it can be measured. This is the part of the pipeline most likely to need revisiting as levels grow — ADR-007 records the fallback.

### Build flow

```
project.json + content/**.json
        │
        ├── (editor / dev build) ──► loadFromJson ──► ContentTables ──► game
        │
        └── (shipping build)
              │  simplish-content-gen: validate → resolve refs → emit C++
              ▼
            generated/content/*.cpp ──► compiled ──► contentTables() ──► game
```

CMake runs the generator as a custom command with the content files as dependencies, so touching one file regenerates and rebuilds only what depends on it. The generator is a host tool depending on nothing but the schema headers, so it cross-compiles trivially and behaves identically on every CI leg.

### Failure modes, and where they surface

| Mistake | Where it is caught |
|---|---|
| Malformed JSON, wrong field type | Editor, on save — the schema validator |
| Reference to a deleted id | Generation — build fails, naming both files |
| Expression using a non-whitelisted property | Generation — build fails |
| Duplicate id within a kind | Generation — build fails |
| Value out of the schema's stated range | Editor, and again at generation |

---

## 10. Versioning and Migration

Each file carries its own schema version. The loader accepts the current version and any older version it has a migration for; the editor writes only the current version, so opening and saving a project upgrades it in place, one file at a time, visible in the diff.

Migrations are code, not configuration: a function per version step, unit-tested against a fixture of the old shape. A file whose version is newer than the build understands is refused with its version in the message rather than partially read.

---

## 11. Deliberately Not in the Format

- **No embedded binary.** Anything large enough to want base64 belongs in `assets/` as a file the pipeline imports.
- **No editor state.** Camera position, selection, panel layout, and bookmarks live in `data/`, are not content, and are not generated. A designer's viewport position is not a property of the level.
- **No absolute paths.** Every path is relative to the project root, so a project moves and clones without rewriting.
- **No derived data.** Flow fields, navigation, collision meshes, and auto-tile resolutions are computed from what is authored. Storing them would mean storing something that can disagree with its source.
- **No general scripting.** See ADR-007, Alternative C.

---

## 12. Implementation Order

The format is specified; none of it is built. A sensible order, each step useful on its own:

1. **Schemas and the envelope** — schema ids, the validator wired to them, and `data/` tables, which are the simplest file kind and already have a runtime consumer.
2. **Level files** — ~~the tile layer, props, entities, and regions~~. Props, lights, player starts and waypoints are done (§4.1): placements, light sources, where players spawn and the routes actors patrol survive a restart. The tile layer, the other entities and regions wait on the tools that author them.
3. **The generator** — starting with data tables and ids, before logic.
4. **Encounters and scenarios** — once the director exists to consume them.
5. **Logic and expressions** — last, because the equivalence test and the expression golden test are what make it safe, and both want the earlier pieces in place.
