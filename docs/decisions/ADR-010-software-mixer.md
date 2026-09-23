# ADR-010: The engine mixes its own audio; a platform supplies only an output

**Status:** Proposed
**Date:** 2026-09-22
**Scope:** Engine | Platform

## Context

Engine REQUIREMENTS named OpenAL Soft, fetched on desktop, behind an
`IAudioBackend` interface that the platform implements with voices, sources,
spatialisation and buses — the shape most engines of this size take, and the
one the sister project voxelina declared. Voxelina fetched OpenAL Soft but
never linked it: its interface was a set of per-source calls with no
implementation behind it, so there was nothing to port but the shape.

The forces:

- **Licensing on consoles.** OpenAL Soft is LGPL. Static linking on a
  console, where a player cannot relink, is not something a closed game can
  ship; every console target would need a second, unrelated implementation of
  the whole interface anyway.
- **Testability.** Voice stealing, priority, ducking and panning are the
  decisions worth testing. Behind an OpenAL-shaped interface they happen in
  the backend, where a headless test can only count calls to a stub.
- **Platform surface.** Every backend of a voice-level interface
  re-implements stealing, buses and ducking, and each can disagree.
- **Budget.** Engine §7 gives the mix ≤ 1.0 ms on a thread of its own. A
  horde shooter's mix is dozens of short one-shots, not hundreds of
  long-lived 3D sources with HRTF; that is well inside a scalar software
  mixer's reach.

## Decision

The engine mixes. `src/engine/audio/` owns clips, a fixed voice pool,
priority stealing, buses, ducking and placement around a listener, and writes
interleaved stereo float frames. `src/platform/audio/` owns one thing: an
output that pulls those frames from its own thread (`AudioDevice`), with
exactly one backend compiled in — SDL3's audio stream on desktop, the
console's own API on a console, none otherwise. The main thread talks to the
mixer through a lock-free single-producer ring, so the device's thread never
waits.

No new dependency: SDL3 is already the desktop platform, and stb_vorbis
comes from the stb checkout already fetched for images.

## Alternatives Considered

### Alternative A: OpenAL Soft behind `IAudioBackend` (the documented plan)

- **How it works:** The platform creates OpenAL sources per sound; OpenAL
  mixes, spatialises and resamples.
- **Pros:** Mature; HRTF, Doppler and EFX reverb available; little mixing
  code in the tree.
- **Cons:** LGPL on consoles; two or three full implementations of voice
  management, one per platform family; the interesting logic is untestable
  headless; 3D features an isometric game does not use.

### Alternative B: miniaudio in the platform layer

- **How it works:** miniaudio's engine (public domain) does the mixing and
  outputs on every desktop OS.
- **Pros:** Permissive licence; one library for decoding, mixing and output.
- **Cons:** Its mixing would live in the platform layer — or in the engine
  as a large third-party dependency whose device code must be stripped — and
  its node graph is far more than a horde shooter needs; consoles still need
  their own output. The output half duplicates SDL3, which the desktop
  already depends on.

### Alternative C: FMOD or Wwise

- **How it works:** A commercial middleware owns the whole audio pipeline and
  an authoring tool.
- **Pros:** Console support, a sound designer's tooling.
- **Cons:** Licence cost and terms; a closed runtime the engine cannot test
  or step through; overkill before there is a sound designer. Reversible
  later: the game speaks `SoundPlay`, which such a backend could accept.

## Design Principle References

- **Principle 1: Determinism Always** — audio is presentation. A mixer on its
  own thread, fed from cues after the tick and never read back, keeps float
  maths and a second thread entirely outside the hashed simulation.
- **Principle 2: Budgets Are Requirements** — the mix allocates nothing after
  construction, and the combat sounds hear only the nearest four cues of each
  kind a tick, so a 2,000-actor volley costs the same voices as a skirmish.
- **Principle 4: Simplicity Over Flexibility** — one mixer, one small
  platform seam, no new libraries.
- **Principle 6: Testability by Construction** — stealing, ducking and
  panning are tested headless on real samples; the SDL backend is tested
  through SDL's dummy driver on machines with no speakers.
- **Principle 7: Platform-Agnostic by Default** — a console port writes one
  function's worth of output, not a voice manager.

## Consequences

### Positive

- Every audible decision is the same on every platform and covered by tests.
- A new target's audio is an output callback, not a subsystem.
- No LGPL code anywhere in the tree.

### Negative

- HRTF, Doppler and convolution reverb are the engine's to write if they are
  ever wanted; OpenAL would have given them away.
- The mixer's performance is the engine's responsibility; it is scalar today.
- A stolen voice is cut rather than faded.

### Implications for Future Work

- Engine REQUIREMENTS' library table and M3 milestone are updated to this
  shape; `IAudioBackend` no longer exists as a planned interface.
- Streaming music, occlusion and reverb go in `engine/audio` as mixer
  features, not in the platform.
- Console backends implement `AudioDevice` in the private overlay, as pad
  backends implement `Gamepads`.
