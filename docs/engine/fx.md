# Effects — Particles, Smoke and Flashes of Light

**Parent document:** [Engine REQUIREMENTS](REQUIREMENTS.md) §5.3, §5.4
**Packages:** `src/engine/render-fx/` (`eng`), `src/game/fx/` (`eng::game`), and the cues in `src/game/combat/`
**Governed by:** [ADR-002](../decisions/ADR-002-fixed-timestep-determinism.md) — effects read the simulation and never write it
**Status:** First slice built and tested. In the editor's playtest, every shot fired throws a muzzle flash, every shot that lands throws sparks off a wall or a spray off whoever it hit, and every blast throws a fireball, embers, smoke and a cloud of volumetric smoke that stands for a few seconds after it; each lights the meshes around it for a moment. There are no weapons yet, so today the shots are the actors'.

The M5 effects system, begun early: what a fight looks like when something happens in it. Decals, projectile trails, screen shake, and the clustered light list are still ahead (§10).

---

## 1. The Shape

```
 tick N ──► GameWorld phases ──► combatCues()            (game/combat: what happened, in order)
                                     │   SHOT_FIRED · SHOT_HIT_BODY · SHOT_HIT_WALL · BLAST
                                     ▼
                   playCombatCues(FxWorld&, cues)         (game/fx: which effect each plays)
                                     │
                     playFxEffect ──► FxParticlePool      (engine/render-fx: bursts of particles)
                                  ├─► FxVolumePool        (clouds of smoke a ray is marched through)
                                  └─► FxLightPool         (flashes that fade)
                                     │
   every frame:  stepFxWorld(seconds)                     (the frame's clock, not the tick's)
                 appendBrightestFxLights ──► MeshLight[]   ──► the scene pass lights meshes by them
                 FxVolumeRenderer::draw ──► buildFxVolumeQuads ──► smoke, after the outline
                 FxRenderer::draw ──► buildFxQuads ──► particles, over the smoke
```

| Piece | Header | What it owns |
|---|---|---|
| `CombatCue`, `CombatCueKind` | `game/combat/combat-cue*.h` | One moment of a fight: what, where, which way, how big, whose side |
| `GameWorld::combatCues` | `game/world/game-world.h` | The last tick's cues, in the order they happened; emptied when the next tick starts |
| `combatCueEffect`, `combatCueEmit`, `playCombatCues` | `game/fx/combat-fx.h` | The built-in effect for each cue kind, and where and which way it plays |
| `FxBurst`, `FxParticleLook`, `FxColor` | `engine/render-fx/fx-burst.h` and friends | A handful of particles thrown out at once, and how each looks and moves |
| `FxFlash` | `engine/render-fx/fx-flash.h` | A point light that flares and fades |
| `FxEffect`, `FxEmit` | `engine/render-fx/fx-effect.h`, `fx-emit.h` | Bursts and a flash played together; where, which way and how big |
| `FxVolume` | `engine/render-fx/fx-volume.h` | One cloud of smoke: a box of procedural noise, how thick, how big, how long |
| `FxParticlePool`, `FxVolumePool`, `FxLightPool` | `engine/render-fx/fx-*-pool.h` | Every live particle, cloud and flash, packed, sized once |
| `FxWorld` | `engine/render-fx/fx-world.h` | All three pools and the `fx` random stream |
| `buildFxQuads`, `FxVertex` | `engine/render-fx/fx-quads.h`, `fx-vertex.h` | Particles laid out as camera-facing quads, farthest first, in clip space |
| `buildFxVolumeQuads`, `FxVolumeVertex` | `engine/render-fx/fx-volume-quads.h`, `fx-volume-vertex.h` | Each cloud laid out as the screen rectangle it covers, carrying the ray its fragments march |
| `FxRenderer`, `FxVolumeRenderer` | `engine/render-fx/fx-renderer.h`, `fx-volume-renderer.h` | The backend's two effects pipelines, three vertex buffers each in turn, one draw a frame each |

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

A burst throws `count` particles inside a cone of `spread_degrees` about the emit direction — 180 throws them every way — each with a speed and a life drawn uniformly between the burst's bounds from the `fx` PCG32 stream (`FX_RNG_STREAM`, Engine §4.3). Every particle of a burst shares its `FxParticleLook`: size and colour at birth and death, gravity (negative rises, as smoke does), drag, `stretch` — the seconds of its own travel it is drawn along as a streak — and three settings that say what the fragment stage makes of it:

| Setting | What it does |
|---|---|
| `spin` | Degrees a second the quad turns. Each particle is thrown at its own angle, drawn from the `fx` stream, so a burst of them never turns as one. Ignored while a particle streaks, since a streak is turned by its motion |
| `shape` | `DISC` is the smooth soft disc; `PUFF` breaks it up with two octaves of value noise, moved by the particle's own seed, so no two puffs are alike. The noise is generated in the shader rather than sampled, which keeps the one texture binding the effects pass has for the scene's depth |
| `lighting` | `EMISSIVE` keeps the particle's own colour, which is what a spark or a fireball wants. `LIT` multiplies its light by what the scene's lights carry to it — the same ambient floor and point falloff the mesh shaders use, worked out per particle on the CPU, with no normal since a particle has no surface. It is what makes smoke and dust sit in a room rather than glow in it |

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

## 5. Volumetric Smoke

A particle is a flat quad. Stand a few of them in a doorway and the illusion breaks: each one cuts against the floor and the wall at a hard line, and none of them fills the space. A `FxVolume` is the other thing — a box of smoke the fragment stage steps a ray through, so it wraps a crate, fills a corridor and thins out where it meets the floor.

One cloud is a box centred on where it was emitted, `radius` across and `height` tall at birth, growing by `growth` tiles a second and drifting up by `rise`. Its `density` is how much light a tile of the thickest part absorbs; `fxVolumeDensity` swells that out of nothing over the first `FX_VOLUME_FADE_IN` of its life and thins it away to nothing by the end, so a cloud never pops in or out. `FX_VOLUME_CAPACITY` is 16 — a march a pixel is dear, so these are counted in tens where particles are counted in thousands — and a cloud past it is dropped rather than stealing one.

### Why this can be done at all

The engine has no 3D textures and no compute builtins, so there is no voxel grid to sample and nothing to fill one with. What it does have is a camera that never rotates and is orthographic (ADR-003), and that is enough:

- **The ray through every pixel runs the same way.** Clip depth grows fastest along the view ray, so the depth row of the view-projection *is* that ray: its direction normalized, and its length the clip depth one tile along it is worth.
- **World position varies affinely with screen position on a fixed plane.** So the CPU can hand the fragment stage a ray by writing four corners and letting the rasterizer interpolate, with no matrix and no inverse in the shader.

`buildFxVolumeQuads` does that work once a cloud, on the CPU: it projects the box's eight corners to find the rectangle it covers on screen, clamps that to the frame, and for each corner unprojects back onto the plane through the cloud's middle — a 3×3 inverse of the matrix's linear part, which is all an orthographic camera needs. Each vertex carries that point and the ray, both in the cloud's own space where its box runs −1 to 1 on every axis, so the fragment stage's slab test and its noise field need no extents of their own. The rectangle is drawn a hair wider than the box (`MARGIN`), which keeps every corner strictly outside it: a ray starting exactly on a wall it is parallel to is the one case a slab test cannot answer, and it is exactly what a camera looking down an axis would produce.

### What a fragment does

Sixteen steps, and at each one two octaves of 3D value noise:

```
slab test against the box            →  t_in, t_out
scene depth, back to a distance      →  t_out = min(t_out, (scene_z - plane_z) / depth_per_tile)
march:  density = ellipsoid(p)² × saturate(fbm(p × 1.9 + seed) × 1.7 − 0.45)
        alpha   = 1 − exp(−density × thickness × dt)
        cover  += through × alpha ;  through × = 1 − alpha
out = colour × cover                                (premultiplied, as every effect is)
```

Cutting the march at the scene's depth is what makes this worth its cost: the smoke stops where a surface is rather than being clipped by it, so it fades into the floor and behind a crate by itself, with no soft-particle fudge. The ellipsoid term takes the cloud to nothing at its box's wall, so the box is never seen.

The pipeline is a backend builtin of its own, `RhiDevice::tryCreateFxVolumePipeline`, drawn in the same pass, with the same premultiplied blend and the same depth texture as the particles — and **before** them, so a spark thrown into a cloud shows through it. Metal, Vulkan, DX12 and OpenGL each embed their own copy; OpenGL brings the depth it reads back to clip space first, since its default range stores half of it. A backend without the pipeline draws particles and no smoke, and says so once in the log.

---

## 6. Flashes

A flash is a point light that starts at its intensity and falls with the square of what is left of its life (`fxLightAt`), so it flares and is quickly gone. It lights meshes through the same `MeshLight` a placed point light is, in the same shader.

The mesh shader's light block holds `MESH_MAX_LIGHTS` (eight) lights per draw. The level's own lights come first; `appendBrightestFxLights` then fills whatever slots are left with the brightest flashes as they shine that frame. A level with no lights of its own is lit by the built-in key light only while its light list is empty, so the editor puts that key light in explicitly before adding any flash — a muzzle flash must never put the level out. Eight is the editor viewport's budget, not the game's: Engine §5.4's 256 dynamic lights need the clustered path, which is a light list in a buffer rather than bytes on a draw.

`FX_LIGHT_CAPACITY` (64) flashes are kept at once. One past it takes the place of whichever is nearest its end, since a new flash matters more to the scene than the last glimmer of an old one.

---

## 7. The Built-in Effects

`game/fx` holds one effect per cue kind. They are built in until effects become content ([ADR-007](../decisions/ADR-007-json-authored-cpp-baked-content.md)), when this table is the thing the content replaces.

A blast is the only cue that leaves a cloud behind, and it is the point of the cloud: the smoke a fight leaves is cover, which a billboard standing in a corridor cannot be. It is four seconds long, where the blast's own puffs last one and a half.

| Cue | Particles | Flash | Played |
|---|---|---|---|
| `SHOT_FIRED` | A short core and a few quick sparks | Warm white, 3 tiles, 70 ms | Along the shot, from where it left the shooter's edge |
| `SHOT_HIT_WALL` | Sparks that bounce back and fall, a puff of grit | White, 1.8 tiles, 80 ms | Back along the shot and up |
| `SHOT_HIT_BODY` | A dark crimson spray | Faint red, 1.5 tiles, 60 ms | On through whoever was hit, and up |
| `BLAST` | A fireball, embers thrown up, smoke that rises and spreads, and a cloud of volumetric smoke that outlasts them all | Warm, 4 tiles, 300 ms | Upward, scaled by radius ÷ `COMBAT_FX_BLAST_RADIUS` |

Engine §5.5 reserves a hue band for hostile projectiles that no cosmetic effect may use. The band is not yet specified; until the projectile renderer settles it, `HOSTILE_SHOT_HUE_MIN_DEGREES` and `…_MAX_DEGREES` (15°–45°) stand for the orange the playtest draws hostile shots in, and `test_combat_fx` fails any effect colour — at birth, halfway or at death — that is a saturated hue inside it. That is why fire here is yellow-white rather than orange.

---

## 8. In the Editor

**Particle emitters** try effects out where they will be seen. General › effects in the asset browser holds a Particle Emitter card; one dropped in the level throws a burst every interval and flashes with it — live in the viewport while the level is being edited, into an `FxWorld` the editor keeps for that, and into the playtest's once Play is pressed. Its properties panel opens on an Effect row that starts it from any of `game/fx`'s presets (`combat-fx-preset.h`: every burst of the combat effects, taken apart, with the flash that goes with it), and lists every number of the burst below it to change from there — including Spin, and Textured and Lit as toggles, so a burst of glowing discs and a burst of scene-lit puffs are two settings apart. An emitter throws bursts only; the volumetric clouds come with the effects that carry them. `EditorEmitterPlayer` does the timing: an emitter bursts the moment it is seen and then on its interval, never faster than `EDITOR_EMITTER_MIN_INTERVAL`, and at most eight bursts in a frame however long the frame was. Emitters are saved with the level as `entity:fx_emitter` ([project-format.md §4.1](../editor/project-format.md#41-what-the-editor-writes-today)) and are presentation only — nothing of them reaches a tick.

Agents reach both halves. `play_effect` fires an effect once, now — a preset burst, a whole combat effect, or an emitter's own burst as edited — into whichever `FxWorld` the viewport is drawing, handed to the running editor as an `EditorEffectShot` with its bursts and its clouds copied in, so `play_effect {"effect": "blast"}` is how an agent puts volumetric smoke in front of the camera; `get_effects` reads `EditorEffectsState`, the counts the editor mirrors into shell state every frame, so an agent can see particles, clouds, flashes and each emitter's bursts while the level is edited as well as while it is played.

The playtest session holds the `FxWorld`, seeded like the run, and plays each tick's cues as soon as the tick has run. The editor steps the effects on the frame's own time while the playtest runs, by one tick's worth per F7 step, and not at all while paused, so a paused frame can be looked at. `get_playtest` reports `effects` — particles, clouds and flashes live now, and how many cues of each kind the run has played — so an agent can see a shot was fired after its spark has gone ([capabilities.md](../editor/capabilities.md)).

---

## 9. Testing

| Test | What it proves |
|---|---|
| `test_fx_particle_pool`, `test_fx_light_pool`, `test_fx_world`, `test_fx_color` | Cones, speeds, lives, gravity, drag, the floor, expiry, fading, eviction, the brightest-first light list, and that playing an effect leaves its clouds standing |
| `test_fx_quads` | Quads round in pixels at one depth, sized and coloured by age, farthest first, streaks along motion, turned by their own spin, and dimmed by the scene's lights only when `LIT` |
| `test_fx_volume_pool` | A cloud's own seed, a full pool dropping rather than stealing, rise and growth and the emit's scale, and density swelling out of nothing and thinning away |
| `test_fx_volume_quads` | The rectangle a cloud covers, its corners landing on the box's walls in the cloud's own space, the ray running down the box's own axis, farthest first, and nothing for a cloud off screen or a camera with no depth |
| `test_fx_renderer` | Against a fake device: the upload, the draw's bindings, buffer rotation, the nearest kept when over capacity, an inert renderer without a pipeline |
| `test_gpu_fx_renderer` | On real Metal, and on Vulkan through MoltenVK: a particle behind the scene's depth is hidden and one in front shows, smoke hides a glow behind it, and a puff reads unevenly where a disc is the same all round — read back from the target |
| `test_gpu_fx_volume_renderer` | On the same two: a cloud in front of the scene's depth covers it and one behind it is gone, and the smoke is uneven across its own face rather than a flat shape |
| `test_combat_system`, `test_world_combat` | Each cue, where it is placed, that a full pool cues nothing, that cues past the reserved room are dropped, that a blast is cued on its tick only, and that the same run cues the same things |
| `test_combat_fx` | Every cue kind has an effect, emits point the right way, no colour uses the hostile hue band, and only a blast leaves a cloud — one that outlasts its puffs and is sized by its radius |
| `test_editor_emitter_ops`, `test_editor_emitter_player`, `test_agent_emitters` | Emitters start from presets and are marked edited once changed, every field reads back and is held to its range, bursts come on the interval and are capped after a stall, and every agent tool that places, changes, moves or removes one |
| `test_editor_playtest_session`, `test_agent_state_json` | A shot fired in a playtest throws particles and a flash; effects age on frame time; aging them never moves a tick hash; `get_playtest` reports them |

DX12 and OpenGL compile here but do not run; their shader text was checked with `glslangValidator` (GLSL 4.60, and HLSL through glslang's HLSL front end).

---

## 10. Not Yet

- **Decals** — scorch and impact marks projected onto the depth buffer (§5.3).
- **Projectile trails**, and the projectiles themselves: the playtest still draws shots as marker boxes.
- **Screen shake**, muzzle kick and hit-stop (Game REQUIREMENTS "Feel").
- **The clustered light list** — 256 dynamic lights rather than eight a draw.
- **The effect budget** of §5.5 — capping additive coverage and culling cosmetics by importance. Today a full pool drops new particles and a full light pool evicts the most faded flash.
- **Effects as content**, authored in JSON and edited in the particle workspace the GUI plan names. The editor's emitters are the first step — a burst edited number by number and saved with a level — but an emitter holds one burst and no cloud, and the combat effects still come from the built-in table rather than from a project's files.
- **Smoke that knows the level.** A cloud is an axis-aligned box of noise standing on its own: it does not pour around a corner, is not pushed by anything, and casts no shadow. Shaped clouds, and smoke that a room's walls hold in, wait on the navigation grid being something the effects pass can read.
- **Cues for everything else** — bites, hazard pools landing, deaths, pickups — and the audio that will read the same cues.
- **GPU simulation.** Particles are simulated on the CPU and drawn in one call; §6 names GPU particles, which wait on a compute path the builtins do not have.
