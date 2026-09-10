#pragma once

/// @file clip-player.h
/// @brief One rig's playback: which clip it plays, and the crossfade from the
/// one it played before.
/// @par Threading
/// Main-thread only (owns mutable storage).

#include <cstddef>
#include <cstdint>
#include <engine/animation/joint-pose.h>
#include <engine/animation/rig-pose.h>
#include <engine/animation/rig.h>
#include <engine/math/mat4.h>
#include <span>
#include <vector>

namespace eng::animation {

/// How long a switch between clips fades when the caller has no better
/// number: a fifth of a second. Long enough that a walk does not snap to an
/// idle, short enough that a character still answers input at once.
inline constexpr float CLIP_DEFAULT_FADE_SECONDS = 0.2f;

/// Plays a rig's clips one at a time, crossfading from each to the next.
///
/// Time is a clock the caller owns, in seconds, passed to every call rather
/// than accumulated here, so any number of players share one clock and a
/// paused clock pauses them all. Each clip loops from its own start, which
/// is the moment `play` chose it.
///
/// A fade from a clip that was simply playing keeps that clip moving while
/// it fades out, so feet carry on walking as the idle takes over. A fade
/// that interrupts another fade has no single clip to fade from — the
/// screen shows a mix of two — so the pose last drawn is frozen and faded
/// from instead. Either way, the first frame after a switch is the frame
/// before it: switching never pops.
///
/// Presentation only, like everything in this package: the simulation never
/// reads a pose (ADR-003 amendment).
/// @thread_safety Main thread only.
class ClipPlayer {
public:
  /// Play clip @p clip from its start at clock time @p now, fading from
  /// what is on screen over @p fade_seconds. Zero or less cuts. Playing the
  /// clip already playing does nothing, and neither is there anything to
  /// fade from before the first `evaluate`, so the first clip always cuts
  /// in. `RIG_REST_POSE` fades to the rest pose.
  void play(size_t clip, double now, float fade_seconds);

  /// Pose @p rig at clock time @p now and return its skin matrices, valid
  /// until the next call.
  std::span<const Mat4> evaluate(const Rig& rig, double now);

  /// The clip playing, or fading in: the last one `play` was given.
  [[nodiscard]] size_t clip() const { return clip_; }

  /// How far the fade into the current clip has got at @p now: zero at the
  /// switch, one once it is complete, eased in and out between. One when
  /// nothing is fading.
  [[nodiscard]] float fadeWeight(double now) const;

private:
  /// Where a fade takes its outgoing pose from.
  enum class FadeSource : uint8_t {
    /// Nothing fading: the current clip alone.
    NONE,
    /// The previous clip, still playing from its own start.
    CLIP,
    /// `frozen_`, the pose on screen when a fade was interrupted.
    FROZEN,
  };

  /// The outgoing pose at @p now, sampled or frozen, into `from_`.
  void sampleFrom(const Rig& rig, double now);

  /// Owns the rig's local, world and skin matrices, and remembers the pose
  /// last shown, which an interrupting `play` freezes.
  RigPose pose_;
  /// The clip playing or fading in.
  size_t clip_ = RIG_REST_POSE;
  /// Clock time `clip_` started.
  double started_ = 0.0;
  /// What the fade into `clip_` fades from.
  FadeSource source_ = FadeSource::NONE;
  /// The clip fading out, when `source_` is `CLIP`.
  size_t from_clip_ = RIG_REST_POSE;
  /// Clock time the clip fading out started, so it carries on from where it
  /// was rather than restarting.
  double from_started_ = 0.0;
  /// Seconds the fade into `clip_` lasts, from `started_`.
  float fade_seconds_ = 0.0f;
  /// The pose frozen by an interrupted fade, when `source_` is `FROZEN`.
  std::vector<JointPose> frozen_;
  /// Scratch: the outgoing pose this frame.
  std::vector<JointPose> from_;
  /// Scratch: the incoming pose this frame, and then the blend of the two.
  std::vector<JointPose> to_;
};

}  // namespace eng::animation
