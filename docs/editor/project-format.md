# Simplish Project Format

**Status:** Specification — not yet implemented
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
│   │   └── transit-station.level.json
│   ├── encounters/
│   │   └── transit-station-waves.encounter.json
│   ├── scenarios/
│   │   └── act-one.scenario.json
│   ├── logic/
│   │   └── transit-station.logic.json
│   └── data/
│       ├── weapons.data.json
│       ├── enemies.data.json
│       └── projectiles.data.json
└── data/                     # editor-owned scratch: layouts, bookmarks
```

`assets/` is source material the asset pipeline consumes ([Editor §8](REQUIREMENTS.md#8-asset-pipeline)). `content/` is authored data this document specifies. The split matters: assets are imported and cached, content is generated and compiled.

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
2. **Level files** — the tile layer, props, entities, and regions, with the editor writing what it already holds in memory. This is what makes placements survive a restart, which today they do not.
3. **The generator** — starting with data tables and ids, before logic.
4. **Encounters and scenarios** — once the director exists to consume them.
5. **Logic and expressions** — last, because the equivalence test and the expression golden test are what make it safe, and both want the earlier pieces in place.
