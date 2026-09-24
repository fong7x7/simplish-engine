#include <engine/render-water/water-corners.h>
#include <engine/render-water/water-surface-mesh.h>
#include <optional>
#include <vector>

namespace eng {

namespace {

  /// The water's cells as the only terrain of a grid of their own, so the
  /// ground's mesh shapes them alone.
  MeshData waterAlone(const GroundGrid& depth) {
    std::vector<uint8_t> cells(depth.cells().size());
    for (size_t i = 0; i < cells.size(); ++i) {
      cells[i] = depth.cells()[i] != 0 ? 1 : 0;
    }
    const std::optional<GroundGrid> alone =
        GroundGrid::fromCells(depth.bounds(), std::move(cells));
    return alone ? makeGroundMesh(*alone, 1) : MeshData{};
  }

}  // namespace

MeshData makeWaterSurfaceMesh(const WaterLayer& layer) {
  MeshData mesh = waterAlone(layer.depth);
  const WaterCorners corners = makeWaterCorners(layer, layer.depth.bounds());
  for (MeshVertex& vertex : mesh.vertices) {
    const WaterSample water =
        waterSampleAt(corners, {vertex.position.x, vertex.position.y});
    vertex.position.z = WATER_SURFACE_HEIGHT;
    vertex.normal = water.color;
    vertex.uv = {water.depth, water.opacity};
  }
  mesh.min.z = mesh.max.z = WATER_SURFACE_HEIGHT;
  return mesh;
}

}  // namespace eng
