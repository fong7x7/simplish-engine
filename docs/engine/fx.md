# Effects — Particles and Flashes of Light

**Parent document:** [Engine REQUIREMENTS](REQUIREMENTS.md) §5.3, §5.4
**Packages:** `src/engine/render-fx/` (`eng`), `src/game/fx/` (`eng::game`), and the cues in `src/game/combat/`
**Governed by:** [ADR-002](../decisions/ADR-002-fixed-timestep-determinism.md) — effects read the simulation and never write it
**Status:** First slice built and tested. In the editor's playtest, every shot fired throws a muzzle flash, every shot that lands throws sparks off a wall or a spray off whoever it hit, and every blast throws a fireball, embers and smoke; each lights the meshes around it for a moment. There are no weapons yet, so today the shots are the actors'.

The M5 effects system, begun early: what a fight looks like when something happens in it. Decals, projectile trails, screen shake, and the clustered light list are still ahead (§7).

---

## 1. The Shape

```
 tick N ──► GameWorld phases ──► combatCues()            (game/combat: what happened, in order)
                                     │   SHOT_FIRED · SHOT_HIT_BODY · SHOT_HIT_WALL · BLAST
                                     ▼
                   playCombatCues(FxWorld&, cues)         (game/fx: which effect each plays)
                                     │
                     playFxEffect ──► FxParticlePool      (engine/render-fx: bursts of particles)
                                  └─► FxLightPool         (flashes that fade)
                                     │
   every frame:  stepFxWorld(seconds)                     (the frame's clock, not the tick's)
                 appendBrightestFxLights ──► MeshLight[]   ──► the scene pass lights meshes by them
                 FxRenderer::draw ──► buildFxQuads ──► one draw, after the outline
```

| Piece | Header | What it owns |
|---|---|---|
| `CombatCue`, `CombatCueKind` | `game/combat/combat-cue*.h` | One moment of a fight: what, where, which way, how big, whose side |
| `GameWorld::combatCues` | `game/world/game-world.h` | The last tick's cues, in the order they happened; emptied when the next tick starts |
| `combatCueEffect`, `combatCueEmit`, `playCombatCues` | `game/fx/combat-fx.h` | The built-in effect for each cue kind, and where and which way it plays |
| `FxBurst`, `FxParticleLook`, `FxColor` | `engine/render-fx/fx-burst.h` and friends | A handful of particles thrown out at once, and how each looks and moves |
| `FxFlash` | `engine/render-fx/fx-flash.h` | A point light that flares and fades |
| `FxEffect`, `FxEmit` | `engine/render-fx/fx-effect.h`, `fx-emit.h` | Bursts and a flash played together; where, which way and how big |
| `FxParticlePool`, `FxLightPool` | `engine/render-fx/fx-*-pool.h` | Every live particle and flash, packed, sized once |
| `FxWorld` | `engine/render-fx/fx-world.h` | Both pools and the `fx` random stream |
| `buildFxQuads`, `FxVertex` | `engine/render-fx/fx-quads.h`, `fx-vertex.h` | Particles laid out as camera-facing quads, farthest first, in clip space |
| `FxRenderer` | `engine/render-fx/fx-renderer.h` | The backend's effects pipeline, three vertex buffers in turn, one draw a frame |

---

## 2. Cues: How the Simulation Says What Happened

Combat appends a `CombatCue` wherever something worth seeing happens: `spawnCombatEffects` for each projectile it spawns, `stepProjectiles` where a projectile reaches a body or a box — the point along its step where it touched — and `resolveBlasts` for each blast. A projectile that simply runs out of flight is not cued.

A cue is an output of the tick and never an input to one:

- **Not state.** No phase reads cues, they are never hashed, and the list is emptied at the start of each tick's first phase. `test_editor_playtest_session` checks that however a playtest's effects are aged, its tick hashes do not move.
- **No allocation.** `cueCombat` appends only into room the world reserved when it was built (every projectile the pool holds landing, plus a volley and a blast per actor) and drops a cue past it. Losing a cue in a crowded tick loses a spark, never a hit.
- **Deterministic anyway.** Cues come from simulation state in phase order, so the same run cues the same things (`test_world_combat`). Nothing depends on that, but it keeps replays and effects in step.

Whoever steps the world reads `combatCues()` after every tick. A frame that runs four ticks reads four lists. When weapons exist they fire through the same combat path, and their shots are cued the same way.

---

## 3. Particles

A burst throws `count` particles inside a cone of `spread_degrees` about the emit direction — 180 throws them every way — each with a speed and a life drawn uniformly between the burst's bounds from the `fx` PCG32 stream (`FX_RNG_STREAM`, Engine §4.3). Every particle of a burst shares its `FxParticleLook`: size and colour at birth and death, gravity (negative rises, as smoke does), drag, and `stretch`, the seconds of its own travel it is drawn along as a streak.

`stepFxParticles` moves them on the render frame's time: gravity, then drag (`exp(-drag * dt)`, so the look does not depend on frame rate), then position. The floor is z = 0 ([Project §8 Q3](../../REQUIREMENTS.md#8-open-questions): one floor plane): a particle reaching it keeps `FX_FLOOR_SKID` of its speed along it and bounces with `FX_FLOOR_BOUNCE` of its fall. A particle that has lived its life is dropped by moving the last live one into its place.

This is presentation, so none of it is under the determinism contract: it calls `cos`, `exp` and `sqrt`, runs on frame time, and a burst past the pool's capacity (`FX_PARTICLE_CAPACITY`, 4,096) simply loses what does not fit.

### Colour

`FxColor` is **premultiplied** linear RGBA, blended `source + destination × (1 − alpha)`. That one blend carries both kinds of particle a burst needs: a glow has alpha zero and only adds light, in any order; smoke is dark with alpha near one and hides what is behind it. The GPU tests check both.

---

## 4. Drawing

Engine §5.3 puts transparent effects after the outline, so that no line lands on a glow. The outline reads the scene's depth as a texture in the pass after the scene's, which has no depth attachment; the effects draw in that same pass, straight after it, and read the same texture:

- **Hidden behind geometry.** Each fragment compares its own depth with the scene's under it and is discarded behind it — without the particle ever writing or testing depth.
- **Soft against it.** Just in front of a surface the particle fades out over `FX_SOFT_TILES` (0.35 tiles along the view ray), so a spark skidding along the floor thins out rather than being cut by it. `FxRenderer::softness` turns that distance into depth units from the matrix's depth row, which under this camera is depth per tile along the ray (§5.1).
- **Round, facing the camera, at one depth.** `buildFxQuads` projects each particle's centre on the CPU and lays its quad out in pixels around it, so it is round on screen whatever the projection, and never leans into the floor. A streaking particle is drawn out along its motion on screen.
- **Farthest first.** Quads are sorted back to front, so smoke hides what is behind it and not what is in front of it. When there are more than a vertex buffer holds, the farthest are the ones left out.
- **One draw.** The vertices are written into one of `FX_FRAME_BUFFER_COUNT` host-visible buffers in turn — the GUI's rotation, for the same frames-in-flight reason — and drawn unindexed. The fragment stage reads one `float4` of parameters at slot 0 and the depth texture at slot 0.

Because the CPU does the projection, the vertex stage only passes each vertex on, and no backend's effects shader needs a matrix. The pipeline is a backend builtin, `RhiDevice::tryCreateFxParticlePipeline`, for the reason the mesh pipeline is one (§5.1). Metal, Vulkan, DX12 and OpenGL each embed their own copy of the shader; OpenGL doubles the depth difference, since its default depth range stores half the clip depth, as its outline does. The stub has none, and there effects still light the scene but draw no particles.

---

## 5. Flashes

A flash is a point light that starts at its intensity and falls with the square of what is left of its life (`fxLightAt`), so it flares and is quickly gone. It lights meshes through the same `MeshLight` a placed point light is, in the same shader.

The mesh shader's light block holds `MESH_MAX_LIGHTS` (eight) lights per draw. The level's own lights come first; `appendBrightestFxLights` then fills whatever slots are left with the brightest flashes as they shine that frame. A level with no lights of its own is lit by the built-in key light only while its light list is empty, so the editor puts that key light in explicitly before adding any flash — a muzzle flash must never put the level out. Eight is the editor viewport's budget, not the game's: Engine §5.4's 256 dynamic lights need the clustered path, which is a light list in a buffer rather than bytes on a draw.

`FX_LIGHT_CAPACITY` (64) flashes are kept at once. One past it takes the place of whichever is nearest its end, since a new flash matters more to the scene than the last glimmer of an old one.

---

## 6. The Built-in Effects

`game/fx` holds one effect per cue kind. They are built in until effects become content ([ADR-007](../decisions/ADR-007-json-authored-cpp-baked-content.md)), when this table is the thing the content replaces.

| Cue | Particles | Flash | Played |
|---|---|---|---|
| `SHOT_FIRED` | A short core and a few quick sparks | Warm white, 3 tiles, 70 ms | Along the shot, from where it left the shooter's edge |
| `SHOT_HIT_WALL` | Sparks that bounce back and fall, a puff of grit | White, 1.8 tiles, 80 ms | Back along the shot and up |
| `SHOT_HIT_BODY` | A dark crimson spray | Faint red, 1.5 tiles, 60 ms | On through whoever was hit, and up |
| `BLAST` | A fireball, embers thrown up, smoke that rises and spreads | Warm, 4 tiles, 300 ms | Upward, scaled by radius ÷ `COMBAT_FX_BLAST_RADIUS` |

Engine §5.5 reserves a hue band for hostile projectiles that no cosmetic effect may use. The band is not yet specified; until the projectile renderer settles it, `HOSTILE_SHOT_HUE_MIN_DEGREES` and `…_MAX_DEGREES` (15°–45°) stand for the orange the playtest draws hostile shots in, and `test_combat_fx` fails any effect colour — at birth, halfway or at death — that is a saturated hue inside it. That is why fire here is yellow-white rather than orange.

---

## 7. In the Editor

**Particle emitters** try effects out where they will be seen. General › effects in the asset browser holds a Particle Emitter card; one dropped in the level throws a burst every interval and flashes with it — live in the viewport while the level is being edited, into an `FxWorld` the editor keeps for that, and into the playtest's once Play is pressed. Its properties panel opens on an Effect row that starts it from any of `game/fx`'s presets (`combat-fx-preset.h`: every burst of the combat effects, taken apart, with the flash that goes with it), and lists every number of the burst below it to change from there. `EditorEmitterPlayer` does the timing: an emitter bursts the moment it is seen and then on its interval, never faster than `EDITOR_EMITTER_MIN_INTERVAL`, and at most eight bursts in a frame however long the frame was. Emitters are saved with the level as `entity:fx_emitter` ([project-format.md §4.1](../editor/project-format.md#41-what-the-editor-writes-today)) and are presentation only — nothing of them reaches a tick.

Agents reach both halves. `play_effect` fires an effect once, now — a preset burst, a whole combat effect, or an emitter's own burst as edited — into whichever `FxWorld` the viewport is drawing, handed to the running editor as an `EditorEffectShot` with its bursts copied in; `get_effects` reads `EditorEffectsState`, the counts the editor mirrors into shell state every frame, so an agent can see particles, flashes and each emitter's bursts while the level is edited as well as while it is played.

The playtest session holds the `FxWorld`, seeded like the run, and plays each tick's cues as soon as the tick has run. The editor steps the effects on the frame's own time while the playtest runs, by one tick's worth per F7 step, and not at all while paused, so a paused frame can be looked at. `get_playtest` reports `effects` — particles and flashes live now, and how many cues of each kind the run has played — so an agent can see a shot was fired after its spark has gone ([capabilities.md](../editor/capabilities.md)).

---

## 8. Testing

| Test | What it proves |
|---|---|
| `test_fx_particle_pool`, `test_fx_light_pool`, `test_fx_world`, `test_fx_color` | Cones, speeds, lives, gravity, drag, the floor, expiry, fading, eviction, the brightest-first light list |
| `test_fx_quads` | Quads round in pixels at one depth, sized and coloured by age, farthest first, streaks along motion |
| `test_fx_renderer` | Against a fake device: the upload, the draw's bindings, buffer rotation, the nearest kept when over capacity, an inert renderer without a pipeline |
| `test_gpu_fx_renderer` | On real Metal, and on Vulkan through MoltenVK: a particle behind the scene's depth is hidden and one in front shows, and smoke hides a glow behind it — read back from the target |
| `test_combat_system`, `test_world_combat` | Each cue, where it is placed, that a full pool cues nothing, that cues past the reserved room are dropped, that a blast is cued on its tick only, and that the same run cues the same things |
| `test_combat_fx` | Every cue kind has an effect, emits point the right way, and no colour uses the hostile hue band |
| `test_editor_emitter_ops`, `test_editor_emitter_player`, `test_agent_emitters` | Emitters start from presets and are marked edited once changed, every field reads back and is held to its range, bursts come on the interval and are capped after a stall, and every agent tool that places, changes, moves or removes one |
| `test_editor_playtest_session`, `test_agent_state_json` | A shot fired in a playtest throws particles and a flash; effects age on frame time; aging them never moves a tick hash; `get_playtest` reports them |

DX12 and OpenGL compile here but do not run; their shader text was checked with `glslangValidator` (GLSL 4.60, and HLSL through glslang's HLSL front end).

---

## 9. Not Yet

- **Decals** — scorch and impact marks projected onto the depth buffer (§5.3).
- **Projectile trails**, and the projectiles themselves: the playtest still draws shots as marker boxes.
- **Screen shake**, muzzle kick and hit-stop (Game REQUIREMENTS "Feel").
- **The clustered light list** — 256 dynamic lights rather than eight a draw.
- **The effect budget** of §5.5 — capping additive coverage and culling cosmetics by importance. Today a full pool drops new particles and a full light pool evicts the most faded flash.
- **Effects as content**, authored in JSON and edited in the particle workspace the GUI plan names. The editor's emitters are the first step — a burst edited number by number and saved with a level — but an emitter holds one burst, and the combat effects still come from the built-in table rather than from a project's files.
- **Cues for everything else** — bites, hazard pools landing, deaths, pickups — and the audio that will read the same cues.
- **GPU simulation.** Particles are simulated on the CPU and drawn in one call; §6 names GPU particles, which wait on a compute path the builtins do not have.
