#include <algorithm>
#include <engine/animation/pose-sampling.h>
#include <engine/animation/rig-pose.h>

namespace eng::animation {

void RigPose::reserveFor(const Rig& rig) {
  const size_t joints = rig.skeleton.parents.size();
  locals_.resize(joints);
  worlds_.resize(joints);
  skin_.resize(rig.skin.joints.size());
}

std::span<const Mat4> RigPose::evaluate(const Rig& rig, size_t clip,
                                        float seconds) {
  reserveFor(rig);
  if (clip < rig.clips.size()) {
    samplePose(rig.skeleton, rig.clips[clip], seconds, locals_);
  } else {
    const size_t count = std::min(rig.skeleton.rest.size(), locals_.size());
    std::copy_n(rig.skeleton.rest.begin(), count, locals_.begin());
  }
  computeJointWorlds(rig.skeleton, locals_, worlds_);
  computeSkinMatrices(rig.skin, worlds_, skin_);
  return skin_;
}

}  // namespace eng::animation
