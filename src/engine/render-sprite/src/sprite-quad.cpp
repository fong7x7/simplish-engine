#include <engine/render-sprite/sprite-quad.h>

namespace eng {

namespace {

  /// Which way the quad is taken to face, for shading — see the header.
  constexpr Vec3 SPRITE_QUAD_NORMAL{0.0f, 1.0f, 0.0f};

  /// Half the quad's width: it is a tile across, centred on its base.
  constexpr float SPRITE_QUAD_HALF_WIDTH = 0.5f;

  /// One corner, at @p x across and @p z up, showing the sheet at @p at.
  MeshVertex corner(float x, float z, Vec2 at) {
    return {{x, 0.0f, z}, SPRITE_QUAD_NORMAL, at};
  }

}  // namespace

MeshData makeSpriteQuadMesh(const SpriteUvRect& uv) {
  const float half = SPRITE_QUAD_HALF_WIDTH;
  MeshData mesh;
  mesh.vertices = {
      corner(-half, 1.0f, {uv.u0, uv.v0}), corner(half, 1.0f, {uv.u1, uv.v0}),
      corner(half, 0.0f, {uv.u1, uv.v1}), corner(-half, 0.0f, {uv.u0, uv.v1})};
  mesh.indices = {0, 1, 2, 0, 2, 3};
  mesh.min = {-half, 0.0f, 0.0f};
  mesh.max = {half, 0.0f, 1.0f};
  return mesh;
}

}  // namespace eng
