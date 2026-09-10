#include <algorithm>
#include <cmath>
#include <editor/shell/mesh-rasterizer.h>
#include <utility>

namespace eng::editor {

namespace {

  constexpr uint32_t RGBA_BYTES = 4;
  /// Background, matching the editor viewport's own fill.
  constexpr uint8_t BG_R = 22;
  constexpr uint8_t BG_G = 22;
  constexpr uint8_t BG_B = 26;
  /// The surface colour every mesh is drawn in, as the mesh shader has it.
  constexpr Vec3 BASE_COLOR{0.74f, 0.76f, 0.80f};
  /// Below this a direction is no direction at all, and the light aimed
  /// along it contributes nothing rather than dividing by zero.
  constexpr float MIN_AIM = 1e-4f;

  /// A vertex after projection: pixel position, depth, and the world-space
  /// position and normal shading needs.
  struct RasterVertex {
    /// Pixel X.
    float x = 0.0f;
    /// Pixel Y.
    float y = 0.0f;
    /// Clip-space depth, smaller being nearer.
    float depth = 0.0f;
    /// World-space position, for the distance a point light falls off over.
    Vec3 world{};
    /// World-space normal, for shading.
    Vec3 normal{};
  };

  /// One pixel about to be written, with everything shading it needs.
  struct RasterFragment {
    /// Index of the pixel in the image.
    size_t index = 0;
    /// Clip-space depth to test against.
    float depth = 0.0f;
    /// World position the pixel covers.
    Vec3 world{};
    /// World normal there.
    Vec3 normal{};
  };

  /// The target being written, with its depth buffer and its lights.
  struct RasterTarget {
    /// Pixels, RGBA8.
    ImageData* image = nullptr;
    /// Depth per pixel, parallel to `image`.
    std::vector<float>* depth = nullptr;
    /// Lights to shade by, never empty: the caller substitutes the built-in
    /// key light for a scene that has none.
    std::span<const MeshLight> lights{};
    /// Tones each light is flattened into; `MESH_SHADE_SMOOTH` for none.
    uint32_t shade_bands = MESH_SHADE_SMOOTH;
  };

  Vec3 transformPoint(const Mat4& m, const Vec3& p) {
    return {m(0, 0) * p.x + m(0, 1) * p.y + m(0, 2) * p.z + m(0, 3),
            m(1, 0) * p.x + m(1, 1) * p.y + m(1, 2) * p.z + m(1, 3),
            m(2, 0) * p.x + m(2, 1) * p.y + m(2, 2) * p.z + m(2, 3)};
  }

  /// A direction through the same transform, which leaves the translation
  /// out. The placement transform is a rotation and a uniform scale, so
  /// this needs no normal matrix — only the normalize that follows it.
  Vec3 transformDirection(const Mat4& m, const Vec3& d) {
    return {m(0, 0) * d.x + m(0, 1) * d.y + m(0, 2) * d.z,
            m(1, 0) * d.x + m(1, 1) * d.y + m(1, 2) * d.z,
            m(2, 0) * d.x + m(2, 1) * d.y + m(2, 2) * d.z};
  }

  /// Project a world position and normal into raster space.
  RasterVertex project(const MeshRasterScene& scene, const Vec3& world,
                       const Vec3& normal) {
    const Vec3 clip = transformPoint(scene.view_projection, world);
    RasterVertex out;
    out.x = (clip.x + 1.0f) * 0.5f * static_cast<float>(scene.width);
    out.y = (1.0f - clip.y) * 0.5f * static_cast<float>(scene.height);
    out.depth = clip.z;
    out.world = world;
    out.normal = normal;
    return out;
  }

  /// How much of a point light reaches a surface this far from it: full at
  /// the light, nothing at its range, and squared in between.
  float falloff(float distance, float range) {
    if (range <= 0.0f) {
      return 0.0f;
    }
    const float reach = std::clamp(1.0f - distance / range, 0.0f, 1.0f);
    return reach * reach;
  }

  /// Which way a light arrives from at @p world, and how much of it is left
  /// by the time it gets there. The vector is not normalized.
  std::pair<Vec3, float> lightAt(const MeshLight& light, const Vec3& world) {
    if (light.kind != MESH_LIGHT_POINT) {
      return {light.direction, 1.0f};
    }
    const Vec3 offset{light.position.x - world.x, light.position.y - world.y,
                      light.position.z - world.z};
    return {offset, falloff(Vec3::length(offset), light.range)};
  }

  /// What one light adds to a surface, per colour channel, with its
  /// strength flattened into @p bands tones as the mesh shader does.
  Vec3 contribution(const MeshLight& light, const Vec3& world,
                    const Vec3& unit_normal, uint32_t bands) {
    const auto [to_light, attenuation] = lightAt(light, world);
    const float aim = Vec3::length(to_light);
    if (aim < MIN_AIM || attenuation <= 0.0f) {
      return {};
    }
    const float lambert =
        std::max(0.0f, Vec3::dot(unit_normal, to_light / aim));
    const float scale = light.intensity *
                        meshShadeBand(lambert * attenuation, bands) *
                        MESH_LIGHT_DIFFUSE;
    return light.color * scale;
  }

  /// The mesh shader's shading term, so the picture reads the same way:
  /// ambient everywhere, plus what each light adds where it reaches.
  Vec3 shade(const RasterTarget& target, const RasterFragment& fragment) {
    const Vec3 unit = Vec3::normalize(fragment.normal);
    Vec3 lit{MESH_LIGHT_AMBIENT, MESH_LIGHT_AMBIENT, MESH_LIGHT_AMBIENT};
    for (const MeshLight& light : target.lights) {
      lit = lit + contribution(light, fragment.world, unit, target.shade_bands);
    }
    return lit;
  }

  /// Signed area of a triangle in raster space, doubled.
  float edge(const RasterVertex& a, const RasterVertex& b, float px, float py) {
    return (px - a.x) * (b.y - a.y) - (py - a.y) * (b.x - a.x);
  }

  /// Barycentric weights of a pixel, or nullopt when it is outside.
  bool coverage(const RasterVertex tri[3], float px, float py, float out[3]) {
    const float area = edge(tri[0], tri[1], tri[2].x, tri[2].y);
    if (std::abs(area) < 1e-6f) {
      return false;
    }
    out[0] = edge(tri[1], tri[2], px, py) / area;
    out[1] = edge(tri[2], tri[0], px, py) / area;
    out[2] = edge(tri[0], tri[1], px, py) / area;
    return out[0] >= 0.0f && out[1] >= 0.0f && out[2] >= 0.0f;
  }

  /// Write one shaded pixel if it passes the depth test.
  void writePixel(const RasterTarget& target, const RasterFragment& fragment) {
    const size_t index = fragment.index;
    if (fragment.depth < 0.0f || fragment.depth > 1.0f ||
        fragment.depth >= (*target.depth)[index]) {
      return;
    }
    (*target.depth)[index] = fragment.depth;
    const Vec3 lit = shade(target, fragment);
    const auto channel = [](float base, float scale) {
      return static_cast<uint8_t>(std::clamp(base * scale, 0.0f, 1.0f) *
                                  255.0f);
    };
    auto& pixels = target.image->pixels;
    pixels[index * RGBA_BYTES + 0] = channel(BASE_COLOR.x, lit.x);
    pixels[index * RGBA_BYTES + 1] = channel(BASE_COLOR.y, lit.y);
    pixels[index * RGBA_BYTES + 2] = channel(BASE_COLOR.z, lit.z);
    pixels[index * RGBA_BYTES + 3] = 255;
  }

  /// Pixel bounds of a triangle, clipped to the image.
  void triangleBounds(const RasterVertex tri[3], const ImageData& image,
                      int32_t out[4]) {
    float min_x = tri[0].x;
    float max_x = tri[0].x;
    float min_y = tri[0].y;
    float max_y = tri[0].y;
    for (int i = 1; i < 3; ++i) {
      min_x = std::min(min_x, tri[i].x);
      max_x = std::max(max_x, tri[i].x);
      min_y = std::min(min_y, tri[i].y);
      max_y = std::max(max_y, tri[i].y);
    }
    out[0] = std::max(0, static_cast<int32_t>(std::floor(min_x)));
    out[1] = std::min(static_cast<int32_t>(image.width),
                      static_cast<int32_t>(std::ceil(max_x)));
    out[2] = std::max(0, static_cast<int32_t>(std::floor(min_y)));
    out[3] = std::min(static_cast<int32_t>(image.height),
                      static_cast<int32_t>(std::ceil(max_y)));
  }

  /// Interpolate three corner values across a pixel's weights.
  Vec3 blend(const Vec3 corners[3], const float weights[3]) {
    return corners[0] * weights[0] + corners[1] * weights[1] +
           corners[2] * weights[2];
  }

  /// What a pixel covers: how deep, where in the world, and which way the
  /// surface faces there.
  ///
  /// All three are interpolated, as the GPU's own interpolation does it: a
  /// point light's distance varies across a face, and so does the normal of
  /// a face that belongs to something round. Taking the first corner's
  /// normal for the whole triangle would face every sphere in the editor
  /// one way per triangle and band it.
  RasterFragment makeFragment(const RasterVertex tri[3],
                              const float weights[3]) {
    const Vec3 positions[3] = {tri[0].world, tri[1].world, tri[2].world};
    const Vec3 normals[3] = {tri[0].normal, tri[1].normal, tri[2].normal};
    RasterFragment out;
    out.depth = weights[0] * tri[0].depth + weights[1] * tri[1].depth +
                weights[2] * tri[2].depth;
    out.world = blend(positions, weights);
    out.normal = blend(normals, weights);
    return out;
  }

  /// Shade one pixel of a triangle, if it is covered by it.
  void shadePixel(const RasterTarget& target, const RasterVertex tri[3],
                  int32_t x, int32_t y) {
    float weights[3] = {0.0f, 0.0f, 0.0f};
    const float px = static_cast<float>(x) + 0.5f;
    const float py = static_cast<float>(y) + 0.5f;
    if (!coverage(tri, px, py, weights)) {
      return;
    }
    RasterFragment fragment = makeFragment(tri, weights);
    fragment.index =
        static_cast<size_t>(y) * target.image->width + static_cast<size_t>(x);
    writePixel(target, fragment);
  }

  /// Rasterize one triangle into the target.
  void fillTriangle(const RasterTarget& target, const RasterVertex tri[3]) {
    int32_t bounds[4] = {0, 0, 0, 0};
    triangleBounds(tri, *target.image, bounds);
    for (int32_t y = bounds[2]; y < bounds[3]; ++y) {
      for (int32_t x = bounds[0]; x < bounds[1]; ++x) {
        shadePixel(target, tri, x, y);
      }
    }
  }

  /// Project and rasterize every triangle of one draw.
  void drawMesh(const RasterTarget& target, const MeshRasterScene& scene,
                const MeshRasterScene::Draw& draw) {
    const MeshData& mesh = *draw.mesh;
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
      RasterVertex tri[3];
      for (size_t corner = 0; corner < 3; ++corner) {
        const MeshVertex& v = mesh.vertices[mesh.indices[i + corner]];
        tri[corner] = project(scene, transformPoint(draw.model, v.position),
                              transformDirection(draw.model, v.normal));
      }
      fillTriangle(target, tri);
    }
  }

  /// Fill the image with the viewport background.
  void fillBackground(ImageData& image) {
    for (size_t i = 0; i < image.pixels.size(); i += RGBA_BYTES) {
      image.pixels[i + 0] = BG_R;
      image.pixels[i + 1] = BG_G;
      image.pixels[i + 2] = BG_B;
      image.pixels[i + 3] = 255;
    }
  }

  /// The image a scene renders into, cleared to the background.
  ImageData makeTargetImage(const MeshRasterScene& scene) {
    ImageData image;
    image.width = scene.width;
    image.height = scene.height;
    image.source_channels = RGBA_BYTES;
    image.pixels.assign(
        static_cast<size_t>(scene.width) * scene.height * RGBA_BYTES, 0);
    fillBackground(image);
    return image;
  }

}  // namespace

ImageData rasterizeMeshScene(const MeshRasterScene& scene) {
  ImageData image = makeTargetImage(scene);
  std::vector<float> depth(static_cast<size_t>(scene.width) * scene.height,
                           1.0f);
  // A default-constructed light is the built-in key light, which is what a
  // scene with none of its own is lit by — see `mesh-light.h`.
  const MeshLight key_light{};
  const RasterTarget target{&image, &depth,
                            scene.lights.empty()
                                ? std::span<const MeshLight>{&key_light, 1}
                                : scene.lights,
                            scene.shade_bands};
  for (const auto& draw : scene.draws) {
    if (draw.mesh != nullptr) {
      drawMesh(target, scene, draw);
    }
  }
  return image;
}

}  // namespace eng::editor
