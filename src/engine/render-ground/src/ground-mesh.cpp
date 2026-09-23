#include <algorithm>
#include <array>
#include <cmath>
#include <engine/render-ground/ground-mesh.h>
#include <engine/render-ground/ground-quarter-shape.h>
#include <iterator>

namespace eng {

namespace {

  /// A quarter turn in radians.
  constexpr float QUARTER_TURN = 1.57079632679489661923f;
  /// Half a tile: the radius of every rounded quarter.
  constexpr float HALF = 0.5f;
  /// How far into its swatch a cell's texture square starts, as a fraction
  /// of the swatch: half a texel, so filtering stays inside it.
  constexpr float SWATCH_INSET =
      0.5f / static_cast<float>(GROUND_SWATCH_TEXELS);

  /// Where one layer's geometry is going: the mesh, the layer, and how many
  /// layers the atlas is divided into.
  struct LayerTarget {
    /// The mesh being built.
    MeshData& mesh;
    /// Which layer, from 1.
    uint8_t layer;
    /// How many layers the atlas holds.
    uint8_t layer_count;
  };

  /// A point on the ground, in world X and Y.
  struct FlatPoint {
    /// World X, in tiles.
    float x = 0.0f;
    /// World Y, in tiles.
    float y = 0.0f;
  };

  /// The vertex at @p point on @p target's layer, textured by where it
  /// falls in @p cell.
  MeshVertex groundVertex(const LayerTarget& target, GroundCell cell,
                          FlatPoint point) {
    const float span = 1.0f - 2.0f * SWATCH_INSET;
    const float local_x = point.x - static_cast<float>(cell.x);
    const float local_y = point.y - static_cast<float>(cell.y);
    const float swatch = static_cast<float>(target.layer - 1) + SWATCH_INSET +
                         (1.0f - local_y) * span;
    return {{point.x, point.y,
             static_cast<float>(target.layer) * GROUND_LAYER_STEP},
            {0.0f, 0.0f, 1.0f},
            {SWATCH_INSET + local_x * span,
             swatch / static_cast<float>(target.layer_count)}};
  }

  /// Add a vertex at @p point of @p cell and return its index.
  uint32_t addVertex(const LayerTarget& target, GroundCell cell,
                     FlatPoint point) {
    target.mesh.vertices.push_back(groundVertex(target, cell, point));
    return static_cast<uint32_t>(target.mesh.vertices.size() - 1);
  }

  /// Whether @p a, @p b, @p c turn counter-clockwise seen from above.
  bool turnsLeft(FlatPoint a, FlatPoint b, FlatPoint c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x) >= 0.0f;
  }

  /// Index the triangle whose corners are @p corners, in that order.
  void addTriangle(MeshData& mesh, const uint32_t (&corners)[3]) {
    mesh.indices.insert(mesh.indices.end(), std::begin(corners),
                        std::end(corners));
  }

  /// A convex fan from @p hub through @p rim in order, sharing its
  /// vertices, and facing up whichever way the rim runs.
  template <size_t N>
  void addFan(const LayerTarget& target, GroundCell cell, FlatPoint hub,
              const std::array<FlatPoint, N>& rim) {
    const bool left = turnsLeft(hub, rim[0], rim[1]);
    const uint32_t centre = addVertex(target, cell, hub);
    uint32_t previous = addVertex(target, cell, rim[0]);
    for (size_t i = 1; i < N; ++i) {
      const uint32_t next = addVertex(target, cell, rim[i]);
      // Wound counter-clockwise from above whichever way the rim runs, so
      // every triangle faces up.
      if (left) {
        addTriangle(target.mesh, {centre, previous, next});
      } else {
        addTriangle(target.mesh, {centre, next, previous});
      }
      previous = next;
    }
  }

  /// Everything about one quarter the shapes are drawn from: its cell, the
  /// cell's centre, and the signs of the corner it reaches.
  struct QuarterFrame {
    /// The cell the quarter belongs to.
    GroundCell cell{};
    /// The cell's centre.
    FlatPoint centre{};
    /// +1 or −1 along X, towards the quarter's corner.
    float dx = 1.0f;
    /// +1 or −1 along Y, towards the quarter's corner.
    float dy = 1.0f;
  };

  /// The quarter's arc, from the point half a tile along X from the centre
  /// round to the point half a tile along Y.
  std::array<FlatPoint, GROUND_ARC_SEGMENTS + 1>
  arcOf(const QuarterFrame& frame) {
    std::array<FlatPoint, GROUND_ARC_SEGMENTS + 1> arc{};
    for (uint32_t step = 0; step <= GROUND_ARC_SEGMENTS; ++step) {
      const float angle = QUARTER_TURN * static_cast<float>(step) /
                          static_cast<float>(GROUND_ARC_SEGMENTS);
      arc[step] = {frame.centre.x + frame.dx * HALF * std::cos(angle),
                   frame.centre.y + frame.dy * HALF * std::sin(angle)};
    }
    return arc;
  }

  /// The quarter's corner of the cell.
  FlatPoint cornerPoint(const QuarterFrame& frame) {
    return {frame.centre.x + frame.dx * HALF, frame.centre.y + frame.dy * HALF};
  }

  /// The whole quarter: a fan from the centre over its three other corners.
  void addFullQuarter(const LayerTarget& target, const QuarterFrame& frame) {
    const std::array<FlatPoint, 3> rim{
        FlatPoint{frame.centre.x + frame.dx * HALF, frame.centre.y},
        cornerPoint(frame),
        FlatPoint{frame.centre.x, frame.centre.y + frame.dy * HALF}};
    addFan(target, frame.cell, frame.centre, rim);
  }

  /// Add the geometry @p shape takes in the quarter @p frame describes.
  void addQuarter(const LayerTarget& target, const QuarterFrame& frame,
                  GroundQuarterShape shape) {
    switch (shape) {
      case GroundQuarterShape::EMPTY:
        return;
      case GroundQuarterShape::FULL:
        addFullQuarter(target, frame);
        return;
      case GroundQuarterShape::ROUND:
        addFan(target, frame.cell, frame.centre, arcOf(frame));
        return;
      case GroundQuarterShape::FILLET:
        addFan(target, frame.cell, cornerPoint(frame), arcOf(frame));
        return;
    }
  }

  /// The whole cell as one quad, for the common case of a cell inside a
  /// painted area: four vertices where four full quarters would take
  /// sixteen.
  void addFullCell(const LayerTarget& target, GroundCell cell) {
    const auto x = static_cast<float>(cell.x);
    const auto y = static_cast<float>(cell.y);
    const std::array<FlatPoint, 3> rim{FlatPoint{x + 1.0f, y},
                                       FlatPoint{x + 1.0f, y + 1.0f},
                                       FlatPoint{x, y + 1.0f}};
    addFan(target, cell, {x, y}, rim);
  }

  /// The shape each quarter of @p cell takes on @p layer, in
  /// `GROUND_QUARTERS` order.
  std::array<GroundQuarterShape, 4> shapesOf(const GroundGrid& grid,
                                             uint8_t layer, GroundCell cell) {
    std::array<GroundQuarterShape, 4> shapes{};
    for (size_t i = 0; i < shapes.size(); ++i) {
      shapes[i] = groundQuarterShape(grid, layer, cell, GROUND_QUARTERS[i]);
    }
    return shapes;
  }

  /// Which quarters of @p cell the layer above @p target's covers
  /// completely, and so need nothing drawn beneath them. A quarter full on
  /// any higher layer is full on the next one up, so that is the only one
  /// asked.
  std::array<bool, 4> hiddenOf(const LayerTarget& target,
                               const GroundGrid& grid, GroundCell cell) {
    std::array<bool, 4> hidden{};
    if (target.layer >= target.layer_count) {
      return hidden;
    }
    const auto above = shapesOf(grid, target.layer + 1, cell);
    for (size_t i = 0; i < hidden.size(); ++i) {
      hidden[i] = above[i] == GroundQuarterShape::FULL;
    }
    return hidden;
  }

  /// Whether every quarter is drawn whole and none is hidden.
  bool wholeCell(const std::array<GroundQuarterShape, 4>& shapes,
                 const std::array<bool, 4>& hidden) {
    for (size_t i = 0; i < shapes.size(); ++i) {
      if (shapes[i] != GroundQuarterShape::FULL || hidden[i]) {
        return false;
      }
    }
    return true;
  }

  /// Add every quarter of @p cell that @p target's layer reaches and the
  /// layer above does not cover.
  void addCell(const LayerTarget& target, const GroundGrid& grid,
               GroundCell cell) {
    const auto shapes = shapesOf(grid, target.layer, cell);
    const auto hidden = hiddenOf(target, grid, cell);
    if (wholeCell(shapes, hidden)) {
      addFullCell(target, cell);
      return;
    }
    const FlatPoint centre{static_cast<float>(cell.x) + HALF,
                           static_cast<float>(cell.y) + HALF};
    for (size_t i = 0; i < shapes.size(); ++i) {
      const GroundQuarter quarter = GROUND_QUARTERS[i];
      const QuarterFrame frame{cell, centre,
                               static_cast<float>(groundQuarterDx(quarter)),
                               static_cast<float>(groundQuarterDy(quarter))};
      addQuarter(target, frame,
                 hidden[i] ? GroundQuarterShape::EMPTY : shapes[i]);
    }
  }

  /// Take the bounding box of what has been added.
  void measureBounds(MeshData& mesh) {
    if (mesh.vertices.empty()) {
      return;
    }
    mesh.min = mesh.vertices.front().position;
    mesh.max = mesh.min;
    for (const MeshVertex& vertex : mesh.vertices) {
      mesh.min = {std::min(mesh.min.x, vertex.position.x),
                  std::min(mesh.min.y, vertex.position.y),
                  std::min(mesh.min.z, vertex.position.z)};
      mesh.max = {std::max(mesh.max.x, vertex.position.x),
                  std::max(mesh.max.y, vertex.position.y),
                  std::max(mesh.max.z, vertex.position.z)};
    }
  }

}  // namespace

MeshData makeGroundMesh(const GroundGrid& grid, uint8_t layer_count) {
  MeshData mesh;
  const GroundRect bounds = grid.bounds();
  // Every quarter a layer draws lies in a cell of the grid's rectangle: a
  // fillet needs painted cells on two sides, and outside the rectangle at
  // least one of them would be outside it too.
  for (uint8_t layer = 1; layer != 0 && layer <= layer_count; ++layer) {
    const LayerTarget target{mesh, layer, layer_count};
    for (int32_t row = 0; row < bounds.height; ++row) {
      for (int32_t column = 0; column < bounds.width; ++column) {
        addCell(target, grid, {bounds.x + column, bounds.y + row});
      }
    }
  }
  measureBounds(mesh);
  return mesh;
}

}  // namespace eng
