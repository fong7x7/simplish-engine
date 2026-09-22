#include <algorithm>
#include <array>
#include <cmath>
#include <engine/render-fx/fx-quads.h>
#include <functional>
#include <numbers>

namespace eng {

namespace {

  /// Shorter than this on screen, in pixels, a particle's motion is too
  /// slight to streak along.
  constexpr float STILL_PIXELS = 1e-3f;

  /// A point or direction in clip space, before any divide.
  struct Clip {
    /// Clip x.
    float x = 0.0f;
    /// Clip y.
    float y = 0.0f;
    /// Clip z.
    float z = 0.0f;
    /// Clip w; one for a point under the orthographic camera, zero for a
    /// direction.
    float w = 0.0f;
  };

  /// @p p under @p m, with @p w as its fourth component: 1 for a point, 0
  /// for a direction.
  Clip project(const Mat4& m, const Vec3& p, float w) {
    return {m(0, 0) * p.x + m(0, 1) * p.y + m(0, 2) * p.z + m(0, 3) * w,
            m(1, 0) * p.x + m(1, 1) * p.y + m(1, 2) * p.z + m(1, 3) * w,
            m(2, 0) * p.x + m(2, 1) * p.y + m(2, 2) * p.z + m(2, 3) * w,
            m(3, 0) * p.x + m(3, 1) * p.y + m(3, 2) * p.z + m(3, 3) * w};
  }

  /// How one quad is laid on screen: the unit direction in pixels its long
  /// axis runs, and its half-length along and across that.
  struct QuadShape {
    /// Long axis, x, in pixels; unit length with `along_y`.
    float along_x = 1.0f;
    /// Long axis, y, in pixels.
    float along_y = 0.0f;
    /// Half its length along the axis, in pixels.
    float half_along = 0.0f;
    /// Half its width across the axis, in pixels.
    float half_across = 0.0f;
  };

  /// Half the width of the live particle at @p i, in pixels: its size at
  /// its age, through how many pixels a tile across the screen covers.
  float halfPixels(const FxParticlePool& pool, uint32_t i,
                   const FxQuadView& view) {
    const FxParticleLook& look = pool.look[i];
    const float t = fxParticleProgress(pool, i);
    const float size =
        (look.size_start + (look.size_end - look.size_start) * t) *
        pool.scale[i];
    return size * std::abs(view.view_projection(0, 0)) * view.width * 0.5f;
  }

  /// Where the live particle at @p i has turned to by now, in radians: the
  /// angle it was thrown at, plus what its spin has added since.
  float angleOf(const FxParticlePool& pool, uint32_t i) {
    const float degrees = pool.angle[i] + pool.look[i].spin * pool.age[i];
    return degrees * std::numbers::pi_v<float> / 180.0f;
  }

  /// The shape of the live particle at @p i: turned to its own angle, or
  /// drawn out along its motion on screen when its look streaks and it is
  /// moving.
  QuadShape shapeOf(const FxParticlePool& pool, uint32_t i,
                    const FxQuadView& view) {
    const float half = halfPixels(pool, i, view);
    const float stretch = pool.look[i].stretch;
    const Clip v = project(view.view_projection, pool.velocity[i], 0.0f);
    const float dx = v.x * view.width * 0.5f;
    const float dy = v.y * view.height * 0.5f;
    const float moving = std::sqrt(dx * dx + dy * dy);
    if (stretch <= 0.0f || moving < STILL_PIXELS) {
      const float turned = angleOf(pool, i);
      return {std::cos(turned), std::sin(turned), half, half};
    }
    return {dx / moving, dy / moving, half + moving * stretch * 0.5f, half};
  }

  /// How much of a point light of @p range reaches @p distance from it:
  /// `mesh_falloff` in every mesh shader, restated for one point rather
  /// than for a surface.
  float reach(float distance, float range) {
    if (range <= 0.0f) {
      return 0.0f;
    }
    const float left = std::clamp(1.0f - distance / range, 0.0f, 1.0f);
    return left * left;
  }

  /// What @p view's lights add at @p at, as a factor on a particle's own
  /// colour: the ambient floor, then what each light carries to it.
  ///
  /// No normal is involved — a particle has no surface — so a light adds
  /// the same wherever it stands, which is what a puff of smoke does in a
  /// scene it has no shadow in.
  Vec3 lightAt(const FxQuadView& view, const Vec3& at) {
    Vec3 lit{MESH_LIGHT_AMBIENT, MESH_LIGHT_AMBIENT, MESH_LIGHT_AMBIENT};
    for (const MeshLight& light : view.lights) {
      const float carried =
          light.kind == MESH_LIGHT_POINT
              ? reach(Vec3::length(light.position - at), light.range)
              : 1.0f;
      lit =
          lit + light.color * (light.intensity * carried * MESH_LIGHT_DIFFUSE);
    }
    return lit;
  }

  /// @p color as the scene's lights leave it. Only the light it adds is
  /// dimmed: how much of what is behind it a particle hides is its own.
  FxColor litColor(const FxColor& color, const FxQuadView& view,
                   const Vec3& at) {
    const Vec3 lit = lightAt(view, at);
    return {color.r * lit.x, color.g * lit.y, color.b * lit.z, color.a};
  }

  /// The corners of a quad as uv, in the order of its two triangles.
  constexpr std::array<std::array<float, 2>, FX_VERTICES_PER_PARTICLE> CORNERS{
      {{-1, -1}, {1, -1}, {1, 1}, {-1, -1}, {1, 1}, {-1, 1}}};

  /// Everything one particle's quad is built from.
  struct QuadInput {
    /// Its centre in clip space.
    Clip centre;
    /// How it lies on screen.
    QuadShape shape;
    /// Its colour now.
    FxColor color;
    /// 1 for a puff, 0 for a disc, and the seed its noise is broken up by.
    float puff = 0.0f;
    /// That seed.
    float seed = 0.0f;
  };

  /// One corner of @p q, at uv (@p u, @p v).
  FxVertex corner(const QuadInput& q, const FxQuadView& view, float u,
                  float v) {
    const QuadShape& s = q.shape;
    const float px =
        s.along_x * u * s.half_along - s.along_y * v * s.half_across;
    const float py =
        s.along_y * u * s.half_along + s.along_x * v * s.half_across;
    // Pixels back to clip units, scaled by w so the offset survives the
    // divide unchanged.
    const float cx = q.centre.x + px * 2.0f / view.width * q.centre.w;
    const float cy = q.centre.y + py * 2.0f / view.height * q.centre.w;
    return {{cx, cy, q.centre.z, q.centre.w},
            {q.color.r, q.color.g, q.color.b, q.color.a},
            {u, v},
            {q.puff, q.seed}};
  }

  /// The colour the live particle at @p i is drawn in now: its look's two
  /// colours mixed for its age, dimmed by the scene's lights when its look
  /// takes them.
  FxColor colorOf(const FxParticlePool& pool, uint32_t i,
                  const FxQuadView& view) {
    const FxParticleLook& look = pool.look[i];
    const FxColor color = mixFxColor(look.color_start, look.color_end,
                                     fxParticleProgress(pool, i));
    return look.lighting == FxParticleLighting::LIT
               ? litColor(color, view, pool.position[i])
               : color;
  }

  /// Append the quad of the live particle at @p i to @p out.
  void appendQuad(const FxParticlePool& pool, uint32_t i,
                  const FxQuadView& view, std::vector<FxVertex>& out) {
    const bool puffed = pool.look[i].shape == FxParticleShape::PUFF;
    const QuadInput q{project(view.view_projection, pool.position[i], 1.0f),
                      shapeOf(pool, i, view), colorOf(pool, i, view),
                      puffed ? 1.0f : 0.0f, pool.angle[i]};
    for (const auto& uv : CORNERS) {
      out.push_back(corner(q, view, uv[0], uv[1]));
    }
  }

  /// Every live particle's depth, farthest first; ties by index.
  void sortByDepth(const FxParticlePool& particles, const Mat4& vp,
                   std::vector<std::pair<float, uint32_t>>& order) {
    order.clear();
    for (uint32_t i = 0; i < particles.live; ++i) {
      const Clip c = project(vp, particles.position[i], 1.0f);
      order.emplace_back(c.w != 0.0f ? c.z / c.w : c.z, i);
    }
    std::ranges::sort(order, std::greater<>());
  }

}  // namespace

void buildFxQuads(const FxParticlePool& particles, const FxQuadView& view,
                  std::vector<std::pair<float, uint32_t>>& order,
                  std::vector<FxVertex>& out) {
  out.clear();
  if (view.width <= 0.0f || view.height <= 0.0f) {
    return;
  }
  sortByDepth(particles, view.view_projection, order);
  for (const auto& entry : order) {
    appendQuad(particles, entry.second, view, out);
  }
}

}  // namespace eng
