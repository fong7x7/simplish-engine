#include <algorithm>
#include <engine/animation/pose-sampling.h>
#include <engine/animation/rig-pose.h>

namespace eng::animation {

void sampleRigPose(const Rig& rig, size_t clip, float seconds,
                   std::span<JointPose> pose) {
  if (clip < rig.clips.size()) {
    samplePose(rig.skeleton, rig.clips[clip], seconds, pose);
    return;
  }
  const size_t count = std::min(rig.skeleton.rest.size(), pose.size());
  std::copy_n(rig.skeleton.rest.begin(), count, pose.begin());
}

void RigPose::reserveFor(const Rig& rig) {
  const size_t joints = rig.skeleton.parents.size();
  locals_.resize(joints);
  worlds_.resize(joints);
  skin_.resize(rig.skin.joints.size());
}

std::span<const Mat4> RigPose::evaluate(const Rig& rig, size_t clip,
                                        float seconds) {
  reserveFor(rig);
  sampleRigPose(rig, clip, seconds, locals_);
  return finish(rig);
}

std::span<const Mat4>
RigPose::evaluateLocals(const Rig& rig, std::span<const JointPose> locals) {
  reserveFor(rig);
  sampleRigPose(rig, RIG_REST_POSE, 0.0f, locals_);
  const size_t count = std::min(locals.size(), locals_.size());
  std::copy_n(locals.begin(), count, locals_.begin());
  return finish(rig);
}

std::span<const Mat4> RigPose::finish(const Rig& rig) {
  computeJointWorlds(rig.skeleton, locals_, worlds_);
  computeSkinMatrices(rig.skin, worlds_, skin_);
  return skin_;
}

}  // namespace eng::animation
