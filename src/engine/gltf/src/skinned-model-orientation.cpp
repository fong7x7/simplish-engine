#include <engine/gltf/skinned-model-orientation.h>

namespace eng::gltf {

namespace {

  /// Model (x, y, z) to world (x, -z, y): Y-up to Z-up.
  Vec3 turnUp(const Vec3& v) {
    return {v.x, -v.z, v.y};
  }

  /// A quarter turn about X whose sine is @p sine: +1 is `turnUp` as a
  /// matrix, and -1 is its inverse.
  Mat4 quarterTurnAboutX(float sine) {
    Mat4 m = Mat4::identity();
    m(1, 1) = 0.0f;
    m(2, 2) = 0.0f;
    m(1, 2) = -sine;
    m(2, 1) = sine;
    return m;
  }

}  // namespace

void orientSkinnedYUpToZUp(SkinnedModel& model) {
  for (SkinnedMeshVertex& v : model.mesh.vertices) {
    v.position = turnUp(v.position);
    v.normal = turnUp(v.normal);
  }
  // Z takes old Y, so min and max carry over; Y takes old -Z, so they swap.
  const Vec3 min = model.mesh.min;
  const Vec3 max = model.mesh.max;
  model.mesh.min = {min.x, -max.z, min.y};
  model.mesh.max = {max.x, -min.z, max.y};
  const Mat4 turn = quarterTurnAboutX(1.0f);
  const Mat4 unturn = quarterTurnAboutX(-1.0f);
  model.rig.skin.root = turn * model.rig.skin.root;
  for (Mat4& inverse_bind : model.rig.skin.inverse_bind) {
    inverse_bind = inverse_bind * unturn;
  }
}

}  // namespace eng::gltf
