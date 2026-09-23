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
- **Cues for everything else** — bites, pools landing, deaths, footsteps —
  wait on the same cues effects are waiting on ([fx.md §10](fx.md#10-not-yet)).
- **Occlusion and reverb.** Walls do not muffle; rooms do not ring.
- **Console backends**, in the private overlay.
