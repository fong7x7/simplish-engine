#pragma once

// Design Summary -- Xbox Series X Audio (XAudio2 Spatial)
// Technical Approach:
// docs/technical-approaches/engine/platform-xbox-series-x/audio.md
//
// Behaviours:
//   - Initialise XAudio2 + ISpatialAudioClient for Windows Sonic
//   - Play spatial sound at 3D position, returns handle
//   - Update spatial audio object position for moving sounds
//   - Stop a playing sound by handle
//   - Set reverb preset (cave, indoor, outdoor, underwater)
//   - Pause all audio on suspend, resume on wake
//   - Tick spatial audio render pass once per frame
//   - Shut down XAudio2, release all voices and spatial objects
//
// Edge Cases:
//   - XAudio2 init failure: return error, engine runs without audio
//   - Windows Sonic unavailable: fall back to stereo XAudio2
//   - All 128 spatial objects in use: return nullopt
//   - XMA2 file missing: fall back to OGG Vorbis
//   - Dolby Atmos license present: system handles automatically
//   - Suspend during playback: pause source voices
//   - Invalid sound handle: log warning, no-op
//
// Invariants:
//   - XAudio2 mastering voice created once at init, never recreated
//   - Spatial render pass called once per frame from audio thread
//   - XMA2 selected automatically when available, OGG Vorbis fallback
//   - IAudioBackend C++ API identical across all platforms
//
// Integration Points:
//   - Engine audio system: XAudio2SpatialBackend implements IAudioBackend
//   - Voxel DDA occlusion: attenuation values passed to audio backend
//   - Lifecycle: suspend/resume pauses/resumes audio

#include "xbox-audio-config.h"
#include "xbox-audio-context.h"
#include "xbox-sound-play-params.h"
#include "xbox-types.h"

#include <cstdint>
#include <engine/core/expected-polyfill.h>
#include <optional>

namespace eng {

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Initialise XAudio2 engine and ISpatialAudioClient for Windows Sonic
/// spatial rendering. Returns context on success.
/// Main thread only.
std::expected<XboxAudioContext, XboxError>
initXboxAudio(const XboxAudioConfig& config);

/// Shut down XAudio2. Releases all source voices, spatial objects,
/// and the mastering voice. Idempotent.
/// Main thread only.
void shutdownXboxAudio(XboxAudioContext& ctx);

/// Play a spatial sound at a 3D position. Returns a handle for
/// position updates and stopping. Returns nullopt if all spatial
/// objects are in use or audio is not initialised.
/// Main thread only.
std::optional<XboxSoundHandle>
playXboxSpatialSound(XboxAudioContext& ctx, const XboxSoundPlayParams& params);

/// Update the world-space position of a playing spatial sound.
/// No-op if the handle is invalid or the sound has finished.
/// Main thread only.
void updateXboxSpatialPosition(XboxAudioContext& ctx, XboxSoundHandle handle,
                               const float position[3]);

/// Stop a playing sound. Releases the spatial audio object.
/// No-op if the handle is invalid.
/// Main thread only.
void stopXboxSound(XboxAudioContext& ctx, XboxSoundHandle handle);

/// Set the reverb preset for the audio environment.
/// Main thread only.
void setXboxReverbPreset(XboxAudioContext& ctx, XboxReverbPreset preset);

/// Pause all audio playback. Called on suspend.
/// Main thread only.
void pauseXboxAudio(XboxAudioContext& ctx);

/// Resume all audio playback. Called on resume.
/// Main thread only.
void resumeXboxAudio(XboxAudioContext& ctx);

/// Tick the spatial audio render pass. Call once per frame.
/// Updates spatial object positions and renders the spatial pass.
/// Main thread only.
void tickXboxAudio(XboxAudioContext& ctx);

}  // namespace eng
