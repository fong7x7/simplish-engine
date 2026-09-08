#include <algorithm>
#include <cmath>
#include <editor/shell/mesh-rasterizer.h>

namespace eng::editor {

namespace {

  constexpr uint32_t RGBA_BYTES = 4;
  /// Background, matching the editor viewport's own fill.
  constexpr uint8_t BG_R = 22;
  constexpr uint8_t BG_G = 22;
  constexpr uint8_t BG_B = 26;

  /// A vertex after projection: pixel position, depth, and world normal.
  struct RasterVertex {
    /// Pixel X.
    float x = 0.0f;
    /// Pixel Y.
    float y = 0.0f;
    /// Clip-space depth, smaller being nearer.
    float depth = 0.0f;
    /// World-space normal, for shading.
    Vec3 normal{};
  };

  /// The target being written, with its depth buffer.
  struct RasterTarget {
    /// Pixels, RGBA8.
    ImageData* image = nullptr;
    /// Depth per pixel, parallel to `image`.
    std::vector<float>* depth = nullptr;
  };

  Vec3 transformPoint(const Mat4& m, const Vec3& p) {
    return {m(0, 0) * p.x + m(0, 1) * p.y + m(0, 2) * p.z + m(0, 3),
            m(1, 0) * p.x + m(1, 1) * p.y + m(1, 2) * p.z + m(1, 3),
            m(2, 0) * p.x + m(2, 1) * p.y + m(2, 2) * p.z + m(2, 3)};
  }

  /// Project a world position and normal into raster space.
  RasterVertex project(const MeshRasterScene& scene, const Vec3& world,
                       const Vec3& normal) {
    const Vec3 clip = transformPoint(scene.view_projection, world);
    RasterVertex out;
    out.x = (clip.x + 1.0f) * 0.5f * static_cast<float>(scene.width);
    out.y = (1.0f - clip.y) * 0.5f * static_cast<float>(scene.height);
    out.depth = clip.z;
    out.normal = normal;
    return out;
  }

  /// The mesh shader's shading term, so the picture reads the same way.
  float shade(const Vec3& normal) {
    const Vec3 light = Vec3::normalize({-0.35f, -0.45f, 0.82f});
    const Vec3 unit = Vec3::normalize(normal);
    const float lambert = std::max(0.0f, Vec3::dot(unit, light));
    return 0.38f + 0.62f * lambert;
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
  void writePixel(const RasterTarget& target, size_t index, float depth,
                  const Vec3& normal) {
    if (depth < 0.0f || depth > 1.0f || depth >= (*target.depth)[index]) {
      return;
    }
    (*target.depth)[index] = depth;
    const float lit = shade(normal);
    const auto channel = [lit](float base) {
      return static_cast<uint8_t>(std::clamp(base * lit, 0.0f, 1.0f) * 255.0f);
    };
    auto& pixels = target.image->pixels;
    pixels[index * RGBA_BYTES + 0] = channel(0.74f);
    pixels[index * RGBA_BYTES + 1] = channel(0.76f);
    pixels[index * RGBA_BYTES + 2] = channel(0.80f);
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

  /// Shade one pixel of a triangle, if it is covered by it.
  void shadePixel(const RasterTarget& target, const RasterVertex tri[3],
                  int32_t x, int32_t y) {
    float weights[3] = {0.0f, 0.0f, 0.0f};
    const float px = static_cast<float>(x) + 0.5f;
    const float py = static_cast<float>(y) + 0.5f;
    if (!coverage(tri, px, py, weights)) {
      return;
    }
    const float depth = weights[0] * tri[0].depth + weights[1] * tri[1].depth +
                        weights[2] * tri[2].depth;
    const auto index =
        static_cast<size_t>(y) * target.image->width + static_cast<size_t>(x);
    writePixel(target, index, depth, tri[0].normal);
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
        tri[corner] =
            project(scene, transformPoint(draw.model, v.position), v.normal);
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

}  // namespace

ImageData rasterizeMeshScene(const MeshRasterScene& scene) {
  ImageData image;
  image.width = scene.width;
  image.height = scene.height;
  image.source_channels = RGBA_BYTES;
  image.pixels.assign(
      static_cast<size_t>(scene.width) * scene.height * RGBA_BYTES, 0);
  fillBackground(image);
  std::vector<float> depth(static_cast<size_t>(scene.width) * scene.height,
                           1.0f);
  const RasterTarget target{&image, &depth};
  for (const auto& draw : scene.draws) {
    if (draw.mesh != nullptr) {
      drawMesh(target, scene, draw);
    }
  }
  return image;
}

}  // namespace eng::editor
