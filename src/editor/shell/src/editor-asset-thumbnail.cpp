#include <algorithm>
#include <editor/shell/editor-asset-thumbnail.h>
#include <editor/shell/iso-view-matrix.h>
#include <span>

namespace eng::editor {

namespace {

  /// Smallest span treated as real, so a flat or single-point mesh does not
  /// divide the fit by zero.
  constexpr float MIN_SPAN = 1e-4f;

  /// The extremes of the mesh's bounding box once projected onto the
  /// isometric plane, in plane units before zoom.
  struct ProjectedBounds {
    /// Least X and Y found.
    IsoPoint min{};
    /// Greatest X and Y found.
    IsoPoint max{};
  };

  /// The square frame a thumbnail is drawn into.
  Rect frameRect(uint32_t size) {
    const auto side = static_cast<float>(size);
    return makeRect(0.0f, 0.0f, side, side);
  }

  /// Where a world point lands on the isometric plane, before zoom. This is
  /// `makeIsoViewProjection`'s own mapping with the zoom and the viewport
  /// centre left out, which is what makes it usable for measuring.
  IsoPoint projectToIsoPlane(const Vec3& world) {
    return {world.x * ISO_TILE_WIDTH,
            world.y * ISO_TILE_DEPTH - world.z * ISO_TILE_RISE};
  }

  /// One corner of an axis-aligned box, chosen by the low bits of @p corner.
  Vec3 boxCorner(const Vec3& min, const Vec3& max, int corner) {
    return {(corner & 1) != 0 ? max.x : min.x,
            (corner & 2) != 0 ? max.y : min.y,
            (corner & 4) != 0 ? max.z : min.z};
  }

  /// Project all eight corners of the mesh's bounds and keep the extremes.
  ///
  /// The corners, not the min and max points themselves: the projection
  /// mixes Y and Z into one screen axis, so the box's own extremes are not
  /// the extremes of its picture.
  ProjectedBounds projectBounds(const Vec3& min, const Vec3& max) {
    const IsoPoint first = projectToIsoPlane(boxCorner(min, max, 0));
    ProjectedBounds out{first, first};
    for (int corner = 1; corner < 8; ++corner) {
      const IsoPoint p = projectToIsoPlane(boxCorner(min, max, corner));
      out.min.x = std::min(out.min.x, p.x);
      out.min.y = std::min(out.min.y, p.y);
      out.max.x = std::max(out.max.x, p.x);
      out.max.y = std::max(out.max.y, p.y);
    }
    return out;
  }

  /// The view that centres @p bounds in the frame and fills it.
  IsoView fitView(const ProjectedBounds& bounds, uint32_t size) {
    const float span_x = std::max(bounds.max.x - bounds.min.x, MIN_SPAN);
    const float span_y = std::max(bounds.max.y - bounds.min.y, MIN_SPAN);
    const float frame = static_cast<float>(size) * ASSET_THUMBNAIL_FILL;
    // The smaller of the two fits, so the longer axis is the one that just
    // fits and neither is cropped.
    const float zoom = std::min(frame / span_x, frame / span_y);
    const IsoPoint focus{(bounds.min.x + bounds.max.x) * 0.5f,
                         (bounds.min.y + bounds.max.y) * 0.5f};
    return {frameRect(size), focus, zoom};
  }

}  // namespace

ImageData renderAssetThumbnail(const MeshData& mesh, uint32_t size) {
  if (mesh.indices.empty() || size == 0) {
    return {};
  }
  const auto side = static_cast<float>(size);
  const IsoView view = fitView(projectBounds(mesh.min, mesh.max), size);
  const MeshRasterScene::Draw draw{&mesh, Mat4::identity()};
  MeshRasterScene scene;
  scene.view_projection =
      makeIsoViewProjection(view, {frameRect(size), side, side});
  scene.draws = std::span<const MeshRasterScene::Draw>(&draw, 1);
  scene.width = size;
  scene.height = size;
  return rasterizeMeshScene(scene);
}

}  // namespace eng::editor
