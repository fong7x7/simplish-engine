#include <algorithm>
#include <array>
#include <cmath>
#include <engine/render-fx/fx-volume-quads.h>
#include <functional>

namespace eng {

namespace {

  /// Smaller than this, a determinant is taken to be zero and the camera
  /// to flatten the world rather than map it.
  constexpr float SINGULAR = 1e-12f;

  /// Farther out than any clip coordinate reaches, so a rectangle that has
  /// taken no corner in yet is empty whichever one comes first.
  constexpr float FAR_OFF = 1e9f;

  /// How much wider than its box a cloud's rectangle is drawn.
  ///
  /// It keeps every corner strictly outside the box, which matters where
  /// the view ray runs down one of the box's own axes: a ray starting
  /// exactly on a wall it is parallel to is the one case the fragment
  /// stage's slab test cannot answer.
  constexpr float MARGIN = 1.002f;

  /// The columns of the inverse of a camera's linear part: what a clip
  /// direction is worth in world tiles.
  struct ClipInverse {
    /// World tiles one unit of clip x is.
    Vec3 x_column{};
    /// World tiles one unit of clip y is.
    Vec3 y_column{};
    /// World tiles one unit of clip depth is.
    Vec3 z_column{};
  };

  /// What every cloud in one frame is laid out with.
  struct FrameSetup {
    /// The world-to-clip matrix the scene was drawn with.
    Mat4 view_projection{};
    /// Its linear part, inverted.
    ClipInverse inverse{};
    /// Where the world origin lands, in clip space.
    Vec3 offset{};
    /// Unit direction the view ray runs, in world space.
    Vec3 view{};
    /// How much clip depth one tile along that ray is worth.
    float depth_per_tile = 0.0f;
  };

  /// The clip-space rectangle a cloud covers, clamped to the frame.
  struct ClipArea {
    /// Smallest clip x any corner of its box reaches.
    float min_x = 0.0f;
    /// Smallest clip y any corner reaches.
    float min_y = 0.0f;
    /// Largest clip x any corner reaches.
    float max_x = 0.0f;
    /// Largest clip y any corner reaches.
    float max_y = 0.0f;
  };

  /// One cloud worked out for this frame: what its six corners are
  /// written from.
  struct VolumeQuad {
    /// What the whole frame shares.
    FrameSetup frame{};
    /// Where the middle of the cloud is, in tiles.
    Vec3 centre{};
    /// Half its size on each world axis, in tiles.
    Vec3 extents{};
    /// The view ray in the cloud's own space, per tile of world distance.
    Vec3 ray{};
    /// The rectangle it covers on screen.
    ClipArea area{};
    /// Clip depth of the plane through its middle.
    float plane_z = 0.0f;
    /// How thick the smoke is now.
    float density = 0.0f;
    /// What its noise field is moved by.
    float seed = 0.0f;
    /// What a full march of it adds and hides.
    FxColor color{};
  };

  /// @p p under @p m, through the divide.
  Vec3 toClip(const Mat4& m, const Vec3& p) {
    const float w = m(3, 0) * p.x + m(3, 1) * p.y + m(3, 2) * p.z + m(3, 3);
    const float k = w != 0.0f ? 1.0f / w : 1.0f;
    return {(m(0, 0) * p.x + m(0, 1) * p.y + m(0, 2) * p.z + m(0, 3)) * k,
            (m(1, 0) * p.x + m(1, 1) * p.y + m(1, 2) * p.z + m(1, 3)) * k,
            (m(2, 0) * p.x + m(2, 1) * p.y + m(2, 2) * p.z + m(2, 3)) * k};
  }

  /// The world point @p clip stands for, @p clip already taken relative to
  /// where the world origin lands.
  Vec3 unproject(const ClipInverse& inverse, const Vec3& clip) {
    return inverse.x_column * clip.x + inverse.y_column * clip.y +
           inverse.z_column * clip.z;
  }

  /// Invert the 3x3 @p m into @p out. False when it is singular, which
  /// leaves @p out as it was.
  bool invert3(const Mat4& m, ClipInverse& out) {
    const Vec3 r0{m(0, 0), m(0, 1), m(0, 2)};
    const Vec3 r1{m(1, 0), m(1, 1), m(1, 2)};
    const Vec3 r2{m(2, 0), m(2, 1), m(2, 2)};
    const Vec3 first = Vec3::cross(r1, r2);
    const float det = Vec3::dot(r0, first);
    if (std::abs(det) < SINGULAR) {
      return false;
    }
    // Each column of an inverse is the cross product of the other two
    // rows, over the determinant.
    out.x_column = first / det;
    out.y_column = Vec3::cross(r2, r0) / det;
    out.z_column = Vec3::cross(r0, r1) / det;
    return true;
  }

  /// Work @p vp out for the frame. False when it has no depth to march
  /// along, or flattens the world.
  bool frameSetupOf(const Mat4& vp, FrameSetup& out) {
    out.view_projection = vp;
    out.offset = {vp(0, 3), vp(1, 3), vp(2, 3)};
    // Clip depth grows fastest the way the camera looks, so its gradient
    // is the view ray, and how long it is is what a tile of it is worth.
    const Vec3 gradient{vp(2, 0), vp(2, 1), vp(2, 2)};
    out.depth_per_tile = Vec3::length(gradient);
    if (out.depth_per_tile < 1e-6f) {
      return false;
    }
    out.view = gradient / out.depth_per_tile;
    return invert3(vp, out.inverse);
  }

  /// The @p k-th corner of the box centred on @p at with half-sizes @p e.
  Vec3 boxCorner(const Vec3& at, const Vec3& e, int k) {
    return {at.x + ((k & 1) != 0 ? e.x : -e.x),
            at.y + ((k & 2) != 0 ? e.y : -e.y),
            at.z + ((k & 4) != 0 ? e.z : -e.z)};
  }

  /// Widen @p area to take in the clip point @p c.
  void widen(ClipArea& area, const Vec3& c) {
    area.min_x = std::min(area.min_x, c.x);
    area.min_y = std::min(area.min_y, c.y);
    area.max_x = std::max(area.max_x, c.x);
    area.max_y = std::max(area.max_y, c.y);
  }

  /// The part of the frame the box centred on @p at with half-sizes @p e
  /// covers. Empty — min at or past max — when none of it is on screen.
  ClipArea clipAreaOf(const Mat4& vp, const Vec3& at, const Vec3& e) {
    ClipArea area{FAR_OFF, FAR_OFF, -FAR_OFF, -FAR_OFF};
    for (int k = 0; k < 8; ++k) {
      widen(area, toClip(vp, boxCorner(at, e, k)));
    }
    area.min_x = std::max(area.min_x, -1.0f);
    area.min_y = std::max(area.min_y, -1.0f);
    area.max_x = std::min(area.max_x, 1.0f);
    area.max_y = std::min(area.max_y, 1.0f);
    return area;
  }

  /// Work the cloud at @p i out for this frame. False when it has shrunk
  /// to nothing or covers no part of the frame.
  bool volumeQuadOf(const FxVolumePool& pool, uint32_t i, const FrameSetup& f,
                    VolumeQuad& out) {
    const Vec3 e = fxVolumeExtents(pool, i);
    if (e.x <= 0.0f || e.y <= 0.0f || e.z <= 0.0f) {
      return false;
    }
    out.frame = f;
    out.centre = pool.position[i];
    out.extents = e;
    out.ray = {f.view.x / e.x, f.view.y / e.y, f.view.z / e.z};
    out.area = clipAreaOf(f.view_projection, out.centre, e * MARGIN);
    out.plane_z = toClip(f.view_projection, out.centre).z;
    out.density = fxVolumeDensity(pool, i);
    out.seed = pool.seed[i];
    out.color = pool.look[i].color;
    return out.area.min_x < out.area.max_x && out.area.min_y < out.area.max_y;
  }

  /// The corner of @p q at clip (@p cx, @p cy), with the ray its fragments
  /// march along.
  FxVolumeVertex cornerAt(const VolumeQuad& q, float cx, float cy) {
    const Vec3 clip{cx, cy, q.plane_z};
    const Vec3 world = unproject(q.frame.inverse, clip - q.frame.offset);
    const Vec3 local{(world.x - q.centre.x) / q.extents.x,
                     (world.y - q.centre.y) / q.extents.y,
                     (world.z - q.centre.z) / q.extents.z};
    return {{cx, cy, q.plane_z, 1.0f},
            {q.color.r, q.color.g, q.color.b, q.color.a},
            {local.x, local.y, local.z, q.seed},
            {q.ray.x, q.ray.y, q.ray.z, q.frame.depth_per_tile},
            {q.plane_z, q.density, 0.0f, 0.0f}};
  }

  /// Append @p q's two triangles to @p out.
  void appendVolume(const VolumeQuad& q, std::vector<FxVolumeVertex>& out) {
    const ClipArea& a = q.area;
    const std::array<std::array<float, 2>, FX_VERTICES_PER_VOLUME> corners{
        {{a.min_x, a.min_y},
         {a.max_x, a.min_y},
         {a.max_x, a.max_y},
         {a.min_x, a.min_y},
         {a.max_x, a.max_y},
         {a.min_x, a.max_y}}};
    for (const auto& c : corners) {
      out.push_back(cornerAt(q, c[0], c[1]));
    }
  }

  /// Every live cloud's depth, farthest first; ties by index.
  void sortByDepth(const FxVolumePool& volumes, const Mat4& vp,
                   std::vector<std::pair<float, uint32_t>>& order) {
    order.clear();
    for (uint32_t i = 0; i < volumes.live; ++i) {
      order.emplace_back(toClip(vp, volumes.position[i]).z, i);
    }
    std::ranges::sort(order, std::greater<>());
  }

}  // namespace

void buildFxVolumeQuads(const FxVolumePool& volumes, const FxQuadView& view,
                        std::vector<std::pair<float, uint32_t>>& order,
                        std::vector<FxVolumeVertex>& out) {
  out.clear();
  order.clear();
  FrameSetup frame;
  if (view.width <= 0.0f || view.height <= 0.0f ||
      !frameSetupOf(view.view_projection, frame)) {
    return;
  }
  sortByDepth(volumes, view.view_projection, order);
  for (const auto& entry : order) {
    VolumeQuad quad;
    if (volumeQuadOf(volumes, entry.second, frame, quad)) {
      appendVolume(quad, out);
    }
  }
}

}  // namespace eng
