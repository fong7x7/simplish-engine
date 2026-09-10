#include <algorithm>
#include <engine/animation/clip-player.h>
#include <engine/animation/pose-sampling.h>
#include <engine/math/math.h>

namespace eng::animation {

namespace {

  /// Seconds into clip @p clip of @p rig, @p elapsed after it started,
  /// looped. Zero for the rest pose, which has no length.
  float clipSeconds(const Rig& rig, size_t clip, double elapsed) {
    return clip < rig.clips.size() ? loopClipTime(rig.clips[clip], elapsed)
                                   : 0.0f;
  }

}  // namespace

void ClipPlayer::play(size_t clip, double now, float fade_seconds) {
  if (clip == clip_) {
    return;
  }
  const bool shown = !pose_.localPoses().empty();
  const bool mid_fade = source_ != FadeSource::NONE && fadeWeight(now) < 1.0f;
  if (!shown || fade_seconds <= 0.0f) {
    source_ = FadeSource::NONE;
  } else if (mid_fade) {
    frozen_.assign(pose_.localPoses().begin(), pose_.localPoses().end());
    source_ = FadeSource::FROZEN;
  } else {
    from_clip_ = clip_;
    from_started_ = started_;
    source_ = FadeSource::CLIP;
  }
  clip_ = clip;
  started_ = now;
  fade_seconds_ = fade_seconds;
}

float ClipPlayer::fadeWeight(double now) const {
  if (source_ == FadeSource::NONE || fade_seconds_ <= 0.0f) {
    return 1.0f;
  }
  const auto progress = static_cast<float>((now - started_) / fade_seconds_);
  // Eased, so the new clip's influence starts and finishes gently rather
  // than with the kink a straight ramp puts at either end.
  return math::smoothstep(progress);
}

void ClipPlayer::sampleFrom(const Rig& rig, double now) {
  from_.resize(rig.skeleton.parents.size());
  if (source_ == FadeSource::FROZEN) {
    sampleRigPose(rig, RIG_REST_POSE, 0.0f, from_);
    std::copy_n(frozen_.begin(), std::min(frozen_.size(), from_.size()),
                from_.begin());
    return;
  }
  sampleRigPose(rig, from_clip_,
                clipSeconds(rig, from_clip_, now - from_started_), from_);
}

std::span<const Mat4> ClipPlayer::evaluate(const Rig& rig, double now) {
  const float weight = fadeWeight(now);
  if (weight >= 1.0f) {
    // The fade is over, so the outgoing clip is forgotten: a later switch
    // fades from this clip alone.
    source_ = FadeSource::NONE;
    return pose_.evaluate(rig, clip_, clipSeconds(rig, clip_, now - started_));
  }
  to_.resize(rig.skeleton.parents.size());
  sampleRigPose(rig, clip_, clipSeconds(rig, clip_, now - started_), to_);
  sampleFrom(rig, now);
  blendPoses(from_, to_, weight, to_);
  return pose_.evaluateLocals(rig, to_);
}

}  // namespace eng::animation
