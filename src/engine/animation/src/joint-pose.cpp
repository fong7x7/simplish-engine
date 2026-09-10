#include <engine/animation/joint-pose.h>
#include <engine/math/math.h>

namespace eng::animation {

Mat4 jointPoseMatrix(const JointPose& pose) {
  Mat4 m = math::quatToMat4(pose.rotation);
  // Scaling first is scaling each column of the rotation, which is cheaper
  // than building a scale matrix to multiply by.
  const float scale[3] = {pose.scale.x, pose.scale.y, pose.scale.z};
  for (size_t column = 0; column < 3; ++column) {
    for (size_t row = 0; row < 3; ++row) {
      m(row, column) *= scale[column];
    }
  }
  m(0, 3) = pose.translation.x;
  m(1, 3) = pose.translation.y;
  m(2, 3) = pose.translation.z;
  return m;
}

}  // namespace eng::animation
