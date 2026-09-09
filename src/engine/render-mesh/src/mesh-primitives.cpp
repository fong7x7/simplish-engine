#include <algorithm>
#include <cmath>
#include <engine/render-mesh/mesh-primitives.h>

namespace eng {

namespace {

  /// Half the unit box's width, which is every round shape's radius.
  constexpr float HALF = 0.5f;
  /// A whole turn in radians.
  constexpr float TAU = 6.28318530717958647692f;
  /// Half a turn, which is a sphere's pole-to-pole sweep.
  constexpr float PI = TAU * 0.5f;

  /// Add one vertex and return its index.
  uint32_t addVertex(MeshData& mesh, const Vec3& position, const Vec3& normal) {
    mesh.vertices.push_back({position, normal});
    return static_cast<uint32_t>(mesh.vertices.size() - 1);
  }

  /// Index one triangle over vertices already added.
  void addTriangle(MeshData& mesh, uint32_t a, uint32_t b, uint32_t c) {
    mesh.indices.push_back(a);
    mesh.indices.push_back(b);
    mesh.indices.push_back(c);
  }

  /// Add a flat-shaded quad from four corners wound counter-clockwise as
  /// seen from outside, which is what makes the cross product below point
  /// away from the shape rather than into it.
  void addQuad(MeshData& mesh, const Vec3 corners[4]) {
    const Vec3 normal = Vec3::normalize(
        Vec3::cross(corners[1] - corners[0], corners[3] - corners[0]));
    const uint32_t base = addVertex(mesh, corners[0], normal);
    for (size_t corner = 1; corner < 4; ++corner) {
      addVertex(mesh, corners[corner], normal);
    }
    addTriangle(mesh, base, base + 1, base + 2);
    addTriangle(mesh, base, base + 2, base + 3);
  }

  /// Add a flat-shaded triangle from three corners wound counter-clockwise
  /// as seen from outside.
  void addFlatTriangle(MeshData& mesh, const Vec3 corners[3]) {
    const Vec3 normal = Vec3::normalize(
        Vec3::cross(corners[1] - corners[0], corners[2] - corners[0]));
    const uint32_t base = addVertex(mesh, corners[0], normal);
    addVertex(mesh, corners[1], normal);
    addVertex(mesh, corners[2], normal);
    addTriangle(mesh, base, base + 1, base + 2);
  }

  /// Grow a box to contain a point.
  void enclose(Vec3& min, Vec3& max, const Vec3& point) {
    min.x = std::min(min.x, point.x);
    min.y = std::min(min.y, point.y);
    min.z = std::min(min.z, point.z);
    max.x = std::max(max.x, point.x);
    max.y = std::max(max.y, point.y);
    max.z = std::max(max.z, point.z);
  }

  /// Take the bounding box of what has been added. Measured rather than
  /// stated, so a shape's bounds cannot disagree with its geometry.
  void measureBounds(MeshData& mesh) {
    if (mesh.vertices.empty()) {
      return;
    }
    mesh.min = mesh.vertices.front().position;
    mesh.max = mesh.min;
    for (const MeshVertex& vertex : mesh.vertices) {
      enclose(mesh.min, mesh.max, vertex.position);
    }
  }

  /// One corner of the unit box, chosen by the low three bits of @p corner:
  /// bit 0 is +X, bit 1 is +Y, bit 2 is the top.
  Vec3 boxCorner(int corner) {
    return {(corner & 1) != 0 ? HALF : -HALF, (corner & 2) != 0 ? HALF : -HALF,
            (corner & 4) != 0 ? 1.0f : 0.0f};
  }

  /// The six faces of the box, each four corner numbers wound
  /// counter-clockwise as seen from outside that face.
  constexpr int BOX_FACES[6][4] = {
      {0, 2, 3, 1},  // bottom, -Z
      {4, 5, 7, 6},  // top, +Z
      {0, 1, 5, 4},  // front, -Y
      {1, 3, 7, 5},  // right, +X
      {3, 2, 6, 7},  // back, +Y
      {2, 0, 4, 6},  // left, -X
  };

  /// Add one of the box's faces.
  void addBoxFace(MeshData& mesh, const int face[4]) {
    const Vec3 corners[4] = {boxCorner(face[0]), boxCorner(face[1]),
                             boxCorner(face[2]), boxCorner(face[3])};
    addQuad(mesh, corners);
  }

  /// The base's four corners in the order they ring it counter-clockwise
  /// seen from above, which is the order that winds each pyramid side
  /// outward.
  constexpr int BASE_RING[4] = {0, 1, 3, 2};

  /// One of the pyramid's four sides: a base edge, and the apex over the
  /// middle of the box.
  void addPyramidSide(MeshData& mesh, int edge) {
    const Vec3 corners[3] = {boxCorner(BASE_RING[edge]),
                             boxCorner(BASE_RING[(edge + 1) % 4]),
                             {0.0f, 0.0f, 1.0f}};
    addFlatTriangle(mesh, corners);
  }

  /// The point at @p angle on the unit box's inscribed circle, at height
  /// @p z. The circle is what both the cylinder and the caps are drawn on.
  Vec3 ringPoint(float angle, float z) {
    return {std::cos(angle) * HALF, std::sin(angle) * HALF, z};
  }

  /// The angle segment @p segment sits at, going the way that winds a ring
  /// counter-clockwise seen from above.
  float segmentAngle(uint32_t segment) {
    return TAU * static_cast<float>(segment) /
           static_cast<float>(MESH_PRIMITIVE_SEGMENTS);
  }

  /// One quad of the cylinder's side, smooth-shaded: the normals are the
  /// directions out from the axis, so the seams between segments do not
  /// read as edges.
  void addCylinderSegment(MeshData& mesh, uint32_t segment) {
    const float from = segmentAngle(segment);
    const float to = segmentAngle(segment + 1);
    const Vec3 out_from{std::cos(from), std::sin(from), 0.0f};
    const Vec3 out_to{std::cos(to), std::sin(to), 0.0f};
    const uint32_t base = addVertex(mesh, ringPoint(from, 0.0f), out_from);
    addVertex(mesh, ringPoint(to, 0.0f), out_to);
    addVertex(mesh, ringPoint(to, 1.0f), out_to);
    addVertex(mesh, ringPoint(from, 1.0f), out_from);
    addTriangle(mesh, base, base + 1, base + 2);
    addTriangle(mesh, base, base + 2, base + 3);
  }

  /// One triangle of a flat cap at height @p z, facing @p normal. The two
  /// caps wind opposite ways, which the normal's sign decides.
  void addCapTriangle(MeshData& mesh, uint32_t segment, const Vec3& normal) {
    const float z = normal.z > 0.0f ? 1.0f : 0.0f;
    const float from = segmentAngle(segment);
    const float to = segmentAngle(segment + 1);
    const uint32_t centre = addVertex(mesh, {0.0f, 0.0f, z}, normal);
    addVertex(mesh, ringPoint(normal.z > 0.0f ? from : to, z), normal);
    addVertex(mesh, ringPoint(normal.z > 0.0f ? to : from, z), normal);
    addTriangle(mesh, centre, centre + 1, centre + 2);
  }

  /// The direction from a sphere's centre to the point at @p ring,
  /// @p segment — which is also that point's normal.
  Vec3 sphereDirection(uint32_t ring, uint32_t segment) {
    const float phi = PI * static_cast<float>(ring) /
                      static_cast<float>(MESH_PRIMITIVE_RINGS);
    const float theta = segmentAngle(segment);
    return {std::sin(phi) * std::cos(theta), std::sin(phi) * std::sin(theta),
            -std::cos(phi)};
  }

  /// Every vertex of the sphere's grid, ring by ring from the bottom pole.
  /// The seam column is repeated so a ring closes without an index that
  /// wraps back to its start.
  void addSphereVertices(MeshData& mesh) {
    for (uint32_t ring = 0; ring <= MESH_PRIMITIVE_RINGS; ++ring) {
      for (uint32_t segment = 0; segment <= MESH_PRIMITIVE_SEGMENTS;
           ++segment) {
        const Vec3 direction = sphereDirection(ring, segment);
        addVertex(
            mesh,
            {direction.x * HALF, direction.y * HALF, HALF + direction.z * HALF},
            direction);
      }
    }
  }

  /// The two triangles of one cell of that grid. The cells against a pole
  /// come out degenerate, which costs two indices and saves a special case.
  void addSphereCell(MeshData& mesh, uint32_t ring, uint32_t segment) {
    const uint32_t stride = MESH_PRIMITIVE_SEGMENTS + 1;
    const uint32_t base = ring * stride + segment;
    addTriangle(mesh, base, base + 1, base + stride + 1);
    addTriangle(mesh, base, base + stride + 1, base + stride);
  }

}  // namespace

MeshData makeCubeMesh() {
  MeshData mesh;
  for (const auto& face : BOX_FACES) {
    addBoxFace(mesh, face);
  }
  measureBounds(mesh);
  return mesh;
}

MeshData makeSphereMesh() {
  MeshData mesh;
  addSphereVertices(mesh);
  for (uint32_t ring = 0; ring < MESH_PRIMITIVE_RINGS; ++ring) {
    for (uint32_t segment = 0; segment < MESH_PRIMITIVE_SEGMENTS; ++segment) {
      addSphereCell(mesh, ring, segment);
    }
  }
  measureBounds(mesh);
  return mesh;
}

MeshData makePyramidMesh() {
  MeshData mesh;
  addBoxFace(mesh, BOX_FACES[0]);
  for (int edge = 0; edge < 4; ++edge) {
    addPyramidSide(mesh, edge);
  }
  measureBounds(mesh);
  return mesh;
}

MeshData makeCylinderMesh() {
  MeshData mesh;
  for (uint32_t segment = 0; segment < MESH_PRIMITIVE_SEGMENTS; ++segment) {
    addCylinderSegment(mesh, segment);
    addCapTriangle(mesh, segment, {0.0f, 0.0f, 1.0f});
    addCapTriangle(mesh, segment, {0.0f, 0.0f, -1.0f});
  }
  measureBounds(mesh);
  return mesh;
}

}  // namespace eng
