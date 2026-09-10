#include <engine/animation/skeleton.h>

namespace eng::animation {

bool skeletonIsOrdered(const Skeleton& skeleton) {
  const size_t count = skeleton.parents.size();
  if (skeleton.rest.size() != count || skeleton.names.size() != count) {
    return false;
  }
  for (size_t joint = 0; joint < count; ++joint) {
    const uint32_t parent = skeleton.parents[joint];
    if (parent != SKELETON_NO_PARENT && parent >= joint) {
      return false;
    }
  }
  return true;
}

}  // namespace eng::animation
