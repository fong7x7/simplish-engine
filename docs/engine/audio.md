# Simplish — Audio

**Parent document:** [Engine REQUIREMENTS](REQUIREMENTS.md)
**Last Updated:** 2026-09-22

Sounds, from a clip on disk (or made from numbers) to the speakers. This
covers the engine's mixer, clips and volume settings (`src/engine/audio/`),
the platform's output (`src/platform/audio/`), the sounds a fight makes
(`src/game/fx/`'s `combat-sounds.h`), and how the editor sets the volume and
plays a project's own sound files. Why the engine mixes for itself
rather than handing voices to OpenAL is
[ADR-010](../decisions/ADR-010-software-mixer.md).

---

## 1. The split

```
 game/fx         ──  what a fight sounds like
   hearCombatCues    of each kind a tick cued, the nearest four
   combatCueSound    → SoundPlay: clip, bus, gain, pitch, place, priority
       │  AudioEngine::play (main thread)
       ▼
 engine/audio    ──  mixing, device-free
   AudioEngine       a bank of clips; requests into a lock-free queue
   AudioMixer        voices, buses, the listener, ducking → stereo floats
       │  AudioEngine::render (the device's thread)
       ▼
 platform/audio  ──  which output exists, and pulling from it
   AudioDevice       SDL3 on desktop; the console's own; none
```

**Audio is presentation (ADR-002).** The simulation never calls it and never
reads it back; nothing here is hashed. Game code turns a tick's cues into
sounds after the tick, exactly as it turns them into particles and rumble.
The mixer is float maths on its own thread, so it may use SIMD and the
platform's `sin` freely — none of it can move a tick hash.

**The engine owns the mix; the platform owns the speakers.** Everything that
decides what is heard — voices, priority, buses, ducking, where a sound sits
between the ears — is in `engine/audio`, platform-free and tested headless on
real samples. A platform supplies only a place for interleaved stereo float
frames to go, pulled from its own thread. That is all a console port writes.

## 2. Clips

An `AudioClip` is decoded audio: 32-bit float samples, mono or stereo,
interleaved, at the rate it was recorded at. The mixer steps through a clip
at the clip's rate over its own, so nothing is resampled on load.

| Source | Function | Notes |
|---|---|---|
| RIFF WAV | `decodeWav` | 8-, 16-, 24- and 32-bit integer PCM and 32-bit float, plain or `WAVE_FORMAT_EXTENSIBLE`; unknown chunks skipped. Compressed WAVs and more than two channels are refused |
| Ogg Vorbis | `decodeOgg` | stb_vorbis, decoded whole. Its implementation is compiled once, in `stb-vorbis-impl.cpp` — from the stb checkout the tree already fetches, so no new dependency |
| Either | `decodeAudio`, `loadAudioFile` | Told apart by their first four bytes (`RIFF`, `OggS`), not the file name |
| Numbers | `synthesize(SynthSpec)` | A tone sweeping between two pitches and white noise through a sweeping low-pass, under an attack and exponential decay; normalised to a 0.9 peak. The same spec makes the same samples |

Every failure is an empty `std::optional`; nothing throws (ADR-001).

**The bank.** An `AudioClipBank` holds clips by name (`combat.blast`) and by
`AudioClipId`, the order they were added in. It only grows. Adding a name
again replaces what it plays from the next sound on, and keeps the old clip
alive for any voice still reading it. Clips are boxed, so a clip's address
never moves: the mixer's thread is handed a pointer when a sound starts and
never touches the bank itself.

## 3. Playing a sound

`AudioEngine::play(SoundPlay)` returns a `SoundId`, zero when it could not
start: the clip is not in the bank, or 512 requests are already waiting for
the mixer. A `SoundPlay` says:

| Field | Meaning |
|---|---|
| `clip` | Which clip, by id |
| `bus` | `EFFECTS`, `MUSIC` or `INTERFACE` — each with its own volume, under the master |
| `gain` | Its own volume; one is as recorded |
| `pitch` | Playback speed; two is an octave up |
| `loop` | `ONCE`, or `LOOP` until stopped |
| `placement` | `FLAT` (both ears, as recorded — music, menus) or `IN_WORLD` at `at` |
| `ducking` | `DUCKS_MUSIC` holds the music bus down while it plays |
| `priority` | 0–255; decides who keeps a voice when they run out |

`stop(SoundId)` fades a sound out over 10 ms so it does not click;
`stopAll()` fades every one. `setBusGain`, `setMasterGain` and `setListener`
take effect from the mixer's next buffer.

## 4. The mixer

`AudioMixer` plays a fixed number of voices — 48 by default — into
interleaved stereo float frames. Each voice is read at its own step (clip
rate over output rate, times pitch) with linear interpolation, scaled by its
gain, its bus, the music duck if it is music, and where it is; the voices are
summed, scaled by the master volume, and clamped to ±1. Nothing is allocated
after construction.

**Voice stealing.** A sound that finds no free voice takes the voice of the
lowest-priority sound playing, the oldest of those if several tie — so long
as that one's priority is no higher than its own. Otherwise the new sound is
the one dropped. A stolen voice is cut, not faded; the voice budget is sized
so that only horde-scale moments steal, and there the cut is buried.

**Ducking.** While any sound marked `DUCKS_MUSIC` plays (and is not fading
out), the music bus moves toward `duck_gain` (0.35) with a 40 ms time
constant, and back to one with an 800 ms one once the last such sound ends.
A blast is heard over the score rather than through it.

**Counts.** `liveVoices`, `stolenVoices` and `droppedSounds` are kept for a
debug view and for tests; `AudioEngine` republishes the live and dropped
counts through atomics after each render.

## 5. Where a sound is heard from

An `AudioListener` is the ears: a point on the ground and the world
direction of screen-right (`input::MoveBasis::right`), so left and right are
the screen's whatever the camera's yaw. In an isometric game the ears are the
player the camera follows, not the camera.

- **Distance** is across the ground, in tiles; height does not count. Full
  volume within `full_tiles` (3), silent past `silent_tiles` (28), and a
  square-law fade between — quick near, slow far.
- **Pan** is how far the sound is along screen-right, as a share of twice
  `full_tiles`, and never past `AUDIO_MAX_PAN` (0.7), so a horde on the left
  is still heard a little on the right.
- **Equal power.** The pan becomes a gain per ear by cos/sin, scaled so the
  centre is one in both: a sound in front is exactly as loud as a flat one.

A sound's placement is fixed when it starts. Nothing here follows a moving
source yet; combat sounds are short enough not to need it.

## 6. Threads

The device calls `AudioEngine::render` from its own thread whenever the
hardware wants samples; everything else on `AudioEngine` is main-thread-only.
The two meet in an `AudioCommandQueue`: a fixed single-producer,
single-consumer ring where each side owns one index and only reads the
other's, so **the device's thread never waits on the main one** — a wait there
is a gap in the sound. A full ring refuses the request (and `play` returns no
id) rather than growing. `render` drains the ring, mixes, and publishes its
counts; nothing else crosses.

Lifetimes: the engine must outlive the device's `close`, which does not
return until the backend's callback has finished and will not run again.
`DesktopGameClient` declares its `AudioDevice` after its `AudioEngine` for
exactly that reason.

## 7. Backends

Exactly one output backend is compiled into `simplish-platform-audio`,
chosen by `ENGINE_AUDIO_BACKEND` ([its CMakeLists](../../src/platform/audio/CMakeLists.txt)):

| Backend | Default on | Output |
|---|---|---|
| `SDL` | Desktop (macOS, Windows, Linux) | An SDL3 audio stream on the default playback device, in the engine's own format, so SDL converts nothing unless the hardware needs it; its callback runs on SDL's audio thread |
| `TEMPEST` | PS5 | From the private console overlay |
| `XAUDIO2` | Xbox | From the private console overlay |
| `NONE` | Anything else | No output; `open` says why. A console build without its overlay lands here and still builds |

A device that will not open is never fatal: `DesktopGameClient` logs "No
sound" and the game plays silent, the engine still accepting requests.

The platform test sets `SDL_AUDIO_DRIVER=dummy`, whose driver pulls samples
in real time with no hardware, so it proves on a CI runner that an open
device mixes, that a sound plays out, and that `close` stops the callback.

## 8. Volume settings

`AudioVolumes` is what a player chooses: a master volume, one for each bus,
and a mute. Each volume is a slider's position, 0 to 1 — not a gain.
`audioVolumeGain` turns it into one by squaring it: loudness is heard on a log
scale, a square follows it closely enough from full down to about -40 dB, and
it reaches true silence at zero, which a decibel curve never does. Halfway on
the slider sounds like half.

`applyAudioVolumes` sends them to an `AudioEngine`: the master gain — zero
while muted, so unmuting brings every level back — and each bus's.

On disk they are a small JSON file a player can edit
(`audio-volumes-json.h`):

```json
{ "master": 1.0, "effects": 1.0, "music": 0.6, "interface": 1.0, "muted": false }
```

Reading is forgiving, as the bindings file is: a volume the file leaves out
stays at full, one outside 0 to 1 is clamped, and anything unreadable is
skipped and reported while the rest still loads.

The editor keeps them in `audio-volumes.json` in the user's application data,
beside `input-bindings.json` — the user's, not the project's — and sets them
on its Sound screen or through `set_volume`
([capabilities §6.3](../editor/capabilities.md#63-sound)).

## 9. What a fight sounds like

`game/fx`'s `combat-sounds.h` is the audio counterpart of `combat-fx.h`:

| Cue | Sound | Gain | Priority |
|---|---|---|---|
| `SHOT_FIRED` | A crack: bright noise closing down fast over a falling knock | 0.55; 0.75 for a player's own | 100; 160 for a player's own |
| `SHOT_HIT_BODY` | A low, dull thud | 0.6 | 120 |
| `SHOT_HIT_WALL` | A short high tick with a falling ring | 0.4 | 80 |
| `BLAST` | A 1.4 s rumble darkening as it rolls away; ducks the music | 1.0 | 220 |

Each is placed where the cue happened, and each plays a shade off true pitch
(±8%) from a hash of where it happened — presentation, so a hash rather than
a random stream — so a burst of the same shot does not sound like a loop.

**Hearing, not playing, every cue.** A horde firing at once is hundreds of
cues in one tick. `hearCombatCues` keeps, of each kind, the four nearest the
listener (`COMBAT_SOUNDS_PER_KIND`) in the order they happened: one pass,
keeping the nearest four of each kind as it goes, so 2,000 cues cost 2,000
small comparisons, not four million.

The clips are synthesised by `loadCombatSounds`. A project can give any of
them its own recording instead: its sounds table
([project-format §8.4](../editor/project-format.md#84-the-sounds-table)) names a
`.wav` or `.ogg` under `assets/` for a slot, and the editor loads it into the
bank over the built-in clip under the same name — so the clip id, and every
sound already handed out, stays good. A file that is gone or will not decode
leaves the built-in sound, and says so.

**In the editor.** A playtest keeps the cues its ticks leave worth hearing
from player 1 (at most 64 waiting), and after each frame's ticks the editor
takes them, moves the ears to player 1 where the frame draws them, and plays
them through the client's `AudioEngine`. Stopping the playtest fades
everything out. `get_playtest`'s `effects.sounds` counts the cues sent to be
heard.

### 9.1 Footsteps

*Timing by animation is §9.2; this section is what is underfoot and what it sounds like.*

A walk is heard too. `game/fx`'s `footstep-*.h` work out when somebody
walking takes a step, what they are standing on, and what that sounds like
for their feet. Presentation, like the combat sounds: they read positions
the tick left and write nothing back.

**Feet and surfaces.** Two closed sets, in `game/content`. A **step set** is
the kind of feet — `default`, `boots`, `bare`, `claws`, `heavy` — named by a
character's or an enemy's `footsteps` in its table, and by an actor prop's
Footsteps row. A **surface** is what the feet land on — `ground`, `grass`,
`dirt`, `sand`, `water`, `stone`, `wood`, `metal`, `cloth`. Surfaces are
materials, not terrains: a road and a paved floor are both stone.

**What is underfoot** (`footstep-surfaces.h`). A `FootstepSurfaces` is the
level's floor as sound sees it: a grid of surfaces — the editor turns each
painted terrain into its surface ([ground.md](ground.md)) — and over it the
**patches**, the boxes of props given a surface of their own (a rug, a deck,
a grate). Feet inside a patch's footprint, and within a quarter tile of its
height, are on it; where patches overlap the later wins; everywhere else is
the grid's cell, and outside it bare ground.

**When a foot lands.** A walker whose clip has footstep events — the rigged characters, whose clips' foot contacts are found automatically (§9.2) — steps when its clip says. Everybody else steps by stride (`footstep-tracker.h`). The tracker follows every
walker by a key that stays theirs — a player's input slot, an actor's place
in the setup — and adds up the ground each covers; each completed stride is
a step. Timed by distance, not animation, so a sprite, a static model and a
rigged one step alike and a faster walker steps more often unasked. A
walker just seen starts half a stride in; one that stopped starts again most
of a stride in, so moving off is heard at once; more than a tile in one tick
is a spawn or a jump, not a step. Strides: default 0.75 tiles, boots 0.8,
bare 0.7, claws 0.45, heavy 1.1 — `stepSetStride`, in `game/content`, since
the simulation steps by the same strides when game logic listens for steps
([logic.md](../game/logic.md)). The sound's steps are presentation's own and
the simulation's are the tick's; they fall at the same stride, not
necessarily on the same frame.

**What it sounds like** (`footstep-sounds.h`). Every surface has a built-in
step, synthesised, under `step.default.<surface>`: a scuff, a swish, a
crunch, a splash, a click, a hollow knock, a clank. A project can record any
pair in the sounds table, `step.<feet>.<surface>`, and a pair it has not
recorded falls back:

```
step.boots.sand → step.boots.ground → step.default.sand → step.default.ground
      own               own                stood in            stood in
```

A step set's own recording plays as recorded; a stood-in clip is shifted to
the feet's pitch — heavy lower, claws higher — so boots still sound unlike
bare feet on the built-ins. Each step set also has its loudness (heavy 0.5,
bare 0.2). Only slots the project actually loaded a file into count as its
own: the bank never forgets a name, and a recording taken away must stop
playing. Steps take the lowest priority of anything (50), so a fight steals
their voices first, and each is a shade off pitch from a hash of where it
fell.

**Hearing.** `hearFootsteps` keeps the six nearest steps each tick
(`FOOTSTEPS_HEARD`), so a horde walking costs what a patrol does. In the
editor a playtest builds the level's surfaces when Play is pressed, follows
every player and actor after each tick, and hands the steps player 1 would
hear to the speakers beside the combat cues; `get_playtest`'s
`effects.footsteps` counts them.

### 9.2 Animation events

A clip or a sprite sheet can make sounds as it plays: a footstep as each foot lands, a clank as armour swings, a crackle on a torch's third frame. In a playtest every rigged model and every sprite billboard is heard at the moments its animation reaches.

**Where the moments come from.** `content/data/animation-events.data.json` ([project-format §8.5](../editor/project-format.md#85-the-animation-events-table)) gives a model's clip events in seconds, and a sprite sheet events by frame. A clip it says nothing about gets a footstep wherever a foot comes down, found from its skeleton ([animation.md §4.5](animation.md#45-events-moments-of-a-clip)) — so a rigged walk steps in time with its feet with nothing written. A row for a clip replaces what was found; an empty row silences a clip the detection hears wrongly.

**What they play.** `footstep` — the feet of whoever is animating on the surface under them, exactly as a stride's step (§9.1); one of the game's sound slots (`combat.blast`, `step.boots.wood`); or a `.wav` or `.ogg` under the project's assets by its path (`sounds/swoosh.wav`), loaded into the bank under `file:<path>` when the table is read. Each has a gain, 0 to 4.

**When they are heard.** Every frame the editor's `EditorPlacementAnimator` notes, for each rigged thing it poses — props, actors, players — the stretch of its clip the frame played; `crossedClipTimes` says which events it reached. A billboard's sheet events are timed the same way: frame *f* comes up at *f* / fps, the sheet loops every frames / fps, over the frame's stretch of the sprite clock. A footstep goes to the playtest, which finds the surface, keeps the nearest six and counts it with the stride's; any other sound plays at once, placed where it happened, at priority 90 — above a footstep, below a fight. A walker whose clip has footstep events is told to the playtest, which stops stepping it by stride, so no step is heard twice. `get_playtest`'s `effects.animation_sounds` counts the rest. Nothing is heard while a playtest is paused — its clips keep looping on the frame clock, but nothing in the frame is moving — and an event naming a footstep slot (`step.boots.wood`) plays as a footstep does, falling back to the nearest recording its feet have.

**Agents.** `list_animation_events` gives every loaded clip's events and where they came from — authored, detected, none — and the table's rows; `set_animation_events` writes or removes one clip's or sheet's row.

## 10. Files and tests

| File | Holds |
|---|---|
| `engine/audio/audio-engine.h` | The main-thread API and `render` |
| `engine/audio/audio-mixer.h`, `mixer-voice.h`, `audio-mixer-config.h` | Voices, stealing, buses, ducking |
| `engine/audio/audio-spatial.h`, `audio-listener.h`, `stereo-gain.h` | Distance, pan, equal-power gains |
| `engine/audio/audio-command-queue.h`, `audio-command.h`, `audio-command-kind.h` | The lock-free ring between threads |
| `engine/audio/audio-clip.h`, `audio-clip-bank.h`, `audio-decode.h`, `audio-synth.h` | Clips, and where they come from |
| `engine/audio/sound-play.h` and its enums | One request to play |
| `engine/audio/audio-volumes.h`, `audio-volumes-json.h`, `audio-muting.h` | Volume settings, their gain curve, and their file |
| `platform/audio/audio-device.h` | The output, one backend a build |
| `game/fx/combat-sounds.h` | The sound each combat cue makes, and which are heard |
| `game/content/step-set.h`, `footstep-surface.h`, `footstep-names.h` | Kinds of feet, surfaces, and their words |
| `game/fx/footstep-surfaces.h`, `footstep-patch.h` | What is underfoot where |
| `game/fx/footstep-tracker.h`, `footstep-gait.h`, `footstep-walker.h`, `footstep-cue.h` | When a walker steps |
| `game/fx/footstep-sounds.h`, `footstep-clip.h`, `footstep-clip-source.h` | Built-in steps, the fallback chain, and how a step is played |
| `editor/shell/editor-footstep-surfaces.h`, `editor-footstep-choices.h` | A level's surfaces from the document; the Surface and Footsteps rows |
| `engine/animation/clip-event-crossing.h`, `clip-window.h`, `foot-contacts.h` | Which marked moments a frame passed; feet found and their contacts |
| `editor/shell/editor-animation-event-table.h` and its entry and event headers | The animation events table |
| `editor/shell/editor-animation-event-ops.h`, `editor-event-hits.h`, `editor-clip-pass.h` | What each clip and sheet plays, which a frame reached, and the stretch of clip each pose played |
| `editor/shell/simplish-editor-animation-events.cpp`, `editor/agent/src/agent-animation-events.cpp` | Hearing them in a playtest; `list_animation_events`, `set_animation_events` |
| `editor/shell/editor-audio-volumes.h` | The user's volumes file |
| `editor/shell/editor-sound-table.h`, `editor-sound-ops.h`, `editor-sound-import.h` | The project's sounds table, loading its files over the built-in sounds, and importing a file |
| `editor/shell/editor-sound-widget.h`, `simplish-editor-sound.cpp` | The Sound screen, and the editor applying and saving what it changes |
| `editor/agent/src/agent-sound.cpp` | `get_sound`, `set_volume`, `set_sound`, `import_sound`, `play_sound` |

| Test | Proves |
|---|---|
| `test_audio_mixer` | On real samples: a flat sound is heard as recorded and ends; stereo stays stereo; pitch and clip rate set the speed; loops loop and stops fade; buses, master and the clamp; stealing takes the oldest of the lowest priority and drops what matters less; ducking goes down and comes back; placement pans and silences |
| `test_audio_spatial` | Distance law, height ignored, pan follows the camera's right, equal power |
| `test_audio_decode` | Every WAV sample format, extensible, skipped chunks, refusals; Ogg rejection; files |
| `test_audio_synth` | Length, peak, silent tail, determinism, a tone's pitch by zero crossings |
| `test_audio_command_queue` | FIFO, full, wrap, and 20,000 commands across two threads in order |
| `test_audio_engine` | Play → render → heard; refusals; commands in order; a full queue |
| `test_audio_device` | SDL's dummy driver: opens twice, closes twice, pulls a sound to its end, stops pulling on close |
| `test_audio_volumes`, `test_audio_volumes_json` | The square law; volumes applied are what is heard; mute keeps the levels; the file round-trips, clamps, and skips what it cannot read |
| `test_combat_sounds` | Every cue has its clip; blast ducks and outranks; own shots louder; pitch spread; the nearest four heard, in order |
| `test_footstep_tracker`, `test_footstep_surfaces`, `test_footstep_sounds`, `test_footstep_names` | A step a stride, claws quicker than heavy feet, none standing still or across a jump; ground, patches and the later patch winning; built-ins for every surface, the fallback chain, a removed recording no longer counting, stood-in pitch; the nearest six heard |
| `test_editor_footstep_surfaces`, `test_editor_footstep_choices`, `test_editor_playtest_session` | Terrains become surfaces and a rug is cloth; the rows' choices; a player walking on painted sand is heard on sand, one standing still is not; a walker a clip steps for takes no stride steps, and a clip's footstep is heard on the surface under it |
| `test_clip_event_crossing`, `test_foot_contacts` | See [animation.md §6](animation.md#6-testing) |
| `test_editor_animation_event_table`, `test_editor_animation_event_ops`, `test_editor_event_hits`, `test_editor_placement_animator`, `test_agent_animation_events` | The table round-trips and skips bad rows; authored beats detected and a walk steps where nobody wrote a thing; a clip's events hit across the loop and a sheet's as its frame comes up; a pose's pass, restarting on a change of clip; the agent tools, a refused write changing nothing |
| `test_editor_sound_table`, `test_editor_sound_ops`, `test_editor_sound_import`, `test_editor_audio_volumes`, `test_editor_sound_widget` | The table round-trips and skips bad rows; a project file replaces a built-in clip under the same id, and a broken one leaves the built-in and says so; an import copies, never overwrites, and refuses what will not decode; the volumes file; the screen skips headings and turns a bar click into a level |
| `test_agent_sound` | Each sound tool, and each refusal changing nothing |
| `test_editor_playtest_session` | A shot fired in a playtest is heard, once, and counted; unheard cues are capped |

## 11. Not yet

- **Recorded sounds in the shipping game.** The editor plays a project's own
  files in the game's sound slots; the baked runtime of
  [ADR-007](../decisions/ADR-007-json-authored-cpp-baked-content.md) does not
  exist yet to carry them.
- **A valid Ogg in the tests.** Decoding is exercised only on bytes it must
  refuse; the tree has no Vorbis encoder to make a fixture, and none is
  checked in.
- **A game's own options screen.** The volumes are set in the editor today;
  the game has no menus yet.
- **Loudness normalisation and format conversion** on import: a file plays as
  it was recorded.
- **Music** — the bus and the duck exist; streaming a long track from disk
  rather than decoding it whole does not.
- **Moving sources** — a sound's place is fixed when it starts.
- **Cues for everything else** — bites, pools landing, deaths —
  wait on the same cues effects are waiting on ([fx.md §10](fx.md#10-not-yet)).
- **An editor panel for animation events.** They are written by an agent's
  `set_animation_events` or by hand; the editor shows and edits them on no
  panel yet — a timeline under the properties panel is the natural place.
- **Events in the game.** Footsteps and animation events play in the
  editor's playtest only, until the game has a runtime that plays sound;
  and while editing, only in a playtest — a prop looping a walk in the
  viewport makes no sound.
- **Events on a clip that is fading out.** Only the clip playing, or fading
  in, is heard; the one it fades from falls silent at the switch.
- **Occlusion and reverb.** Walls do not muffle; rooms do not ring.
- **Console backends**, in the private overlay.
