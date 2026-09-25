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

  /// Whether any cell of @p depth within one of @p cell holds water.
  bool nearWater(const GroundGrid& depth, GroundCell cell) {
    for (int32_t dy = -1; dy <= 1; ++dy) {
      for (int32_t dx = -1; dx <= 1; ++dx) {
        if (depth.at({cell.x + dx, cell.y + dy}) != 0) {
          return true;
        }
      }
    }
    return false;
  }

  /// Every cell within one of the water, as the only terrain of a grid of
  /// its own: the wet band's shape, the water's own cells included.
  MeshData bandAround(const GroundGrid& depth) {
    const GroundRect in = depth.bounds();
    const GroundRect out{in.x - 1, in.y - 1, in.width + 2, in.height + 2};
    std::vector<uint8_t> cells(static_cast<size_t>(out.width) *
                               static_cast<size_t>(out.height));
    for (int32_t y = 0; y < out.height; ++y) {
      for (int32_t x = 0; x < out.width; ++x) {
        cells[static_cast<size_t>(y * out.width + x)] =
            nearWater(depth, {out.x + x, out.y + y}) ? 1 : 0;
      }
    }
    const std::optional<GroundGrid> band =
        GroundGrid::fromCells(out, std::move(cells));
    return band ? makeGroundMesh(*band, 1) : MeshData{};
  }

  /// @p mesh's vertices at the surface's height, each carrying the water
  /// @p corners give there.
  void soak(MeshData& mesh, const WaterCorners& corners) {
    for (MeshVertex& vertex : mesh.vertices) {
      const WaterSample water =
          waterSampleAt(corners, {vertex.position.x, vertex.position.y});
      vertex.position.z = WATER_SURFACE_HEIGHT;
      vertex.normal = water.color;
      vertex.uv = {water.depth, water.opacity};
    }
  }

  /// @p top's triangles after @p mesh's.
  void append(MeshData& mesh, const MeshData& top) {
    const auto base = static_cast<uint32_t>(mesh.vertices.size());
    mesh.vertices.insert(mesh.vertices.end(), top.vertices.begin(),
                         top.vertices.end());
    for (const uint32_t index : top.indices) {
      mesh.indices.push_back(base + index);
    }
  }

}  // namespace

MeshData makeWaterSurfaceMesh(const WaterLayer& layer) {
  if (layer.depth.empty()) {
    return {};
  }
  MeshData mesh = bandAround(layer.depth);
  for (MeshVertex& vertex : mesh.vertices) {
    vertex.position.z = WATER_SURFACE_HEIGHT;
    vertex.normal = {};
    vertex.uv = {};
  }
  MeshData water = waterAlone(layer.depth);
  soak(water, makeWaterCorners(layer, layer.depth.bounds()));
  append(mesh, water);
  mesh.min.z = mesh.max.z = WATER_SURFACE_HEIGHT;
  return mesh;
}

}  // namespace eng
