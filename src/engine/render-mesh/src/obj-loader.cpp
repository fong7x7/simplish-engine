#include <charconv>
#include <cstddef>
#include <engine/render-mesh/obj-loader.h>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace eng {

namespace {

  /// One face corner: indices into the position and normal arrays, already
  /// resolved to zero-based. `normal` is -1 when the corner declared none.
  struct ObjCorner {
    /// Zero-based index into the position array.
    int32_t position = -1;
    /// Zero-based index into the normal array, or -1.
    int32_t normal = -1;
  };

  /// Everything read from the file, before it becomes a MeshData.
  struct ObjSource {
    /// Positions in `v` order.
    std::vector<Vec3> positions;
    /// Normals in `vn` order.
    std::vector<Vec3> normals;
    /// Corners of each face, in file order.
    std::vector<std::vector<ObjCorner>> faces;
  };

  /// Parse a float, returning 0 for anything unparseable. OBJ files in the
  /// wild carry stray tokens; a bad number should not abort the load.
  float parseFloat(std::string_view token) {
    float value = 0.0f;
    const char* first = token.data();
    const char* last = first + token.size();
    std::from_chars(first, last, value);
    return value;
  }

  /// Resolve an OBJ index to zero-based. OBJ counts from 1, and a negative
  /// index counts back from the end of what has been declared so far.
  int32_t resolveIndex(std::string_view token, size_t declared) {
    if (token.empty()) {
      return -1;
    }
    int32_t raw = 0;
    std::from_chars(token.data(), token.data() + token.size(), raw);
    if (raw > 0) {
      return raw - 1;
    }
    if (raw < 0) {
      return static_cast<int32_t>(declared) + raw;
    }
    return -1;
  }

  /// Split a face corner on '/' into its position and normal slots.
  ObjCorner parseCorner(std::string_view token, const ObjSource& src) {
    const size_t first_slash = token.find('/');
    if (first_slash == std::string_view::npos) {
      return {resolveIndex(token, src.positions.size()), -1};
    }
    const std::string_view position = token.substr(0, first_slash);
    const std::string_view rest = token.substr(first_slash + 1);
    const size_t second_slash = rest.find('/');
    const std::string_view normal = (second_slash == std::string_view::npos)
                                        ? std::string_view{}
                                        : rest.substr(second_slash + 1);
    return {resolveIndex(position, src.positions.size()),
            resolveIndex(normal, src.normals.size())};
  }

  /// Read a `v` or `vn` line's three components.
  Vec3 parseVec3(const std::vector<std::string_view>& tokens) {
    Vec3 out;
    out.x = tokens.size() > 1 ? parseFloat(tokens[1]) : 0.0f;
    out.y = tokens.size() > 2 ? parseFloat(tokens[2]) : 0.0f;
    out.z = tokens.size() > 3 ? parseFloat(tokens[3]) : 0.0f;
    return out;
  }

  /// Split a line on whitespace.
  std::vector<std::string_view> splitTokens(std::string_view line) {
    std::vector<std::string_view> tokens;
    size_t pos = 0;
    while (pos < line.size()) {
      while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
        ++pos;
      }
      const size_t start = pos;
      while (pos < line.size() && line[pos] != ' ' && line[pos] != '\t') {
        ++pos;
      }
      if (pos > start) {
        tokens.push_back(line.substr(start, pos - start));
      }
    }
    return tokens;
  }

  /// Add one `f` line's corners to the source.
  void readFace(const std::vector<std::string_view>& tokens, ObjSource& src) {
    std::vector<ObjCorner> corners;
    corners.reserve(tokens.size() - 1);
    for (size_t i = 1; i < tokens.size(); ++i) {
      corners.push_back(parseCorner(tokens[i], src));
    }
    if (corners.size() >= 3) {
      src.faces.push_back(std::move(corners));
    }
  }

  /// Dispatch one line by its leading keyword.
  void readLine(std::string_view line, ObjSource& src) {
    const auto tokens = splitTokens(line);
    if (tokens.empty() || tokens[0].starts_with('#')) {
      return;
    }
    if (tokens[0] == "v") {
      src.positions.push_back(parseVec3(tokens));
    } else if (tokens[0] == "vn") {
      src.normals.push_back(parseVec3(tokens));
    } else if (tokens[0] == "f") {
      readFace(tokens, src);
    }
  }

  /// Read the whole file into positions, normals, and faces.
  ObjSource readSource(std::string_view text) {
    ObjSource src;
    size_t start = 0;
    while (start <= text.size()) {
      size_t end = text.find('\n', start);
      if (end == std::string_view::npos) {
        end = text.size();
      }
      std::string_view line = text.substr(start, end - start);
      if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);
      }
      readLine(line, src);
      start = end + 1;
    }
    return src;
  }

  /// Key for sharing a vertex between faces that agree on position and
  /// normal. Faces without a normal never share, so they are not keyed.
  uint64_t vertexKey(const ObjCorner& corner) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(corner.position))
            << 32U) |
           static_cast<uint32_t>(corner.normal);
  }

  /// Geometric normal of a triangle, used when the face declared none.
  Vec3 faceNormal(const Vec3& a, const Vec3& b, const Vec3& c) {
    const Vec3 edge0 = b - a;
    const Vec3 edge1 = c - a;
    const Vec3 crossed = Vec3::cross(edge0, edge1);
    return (Vec3::length(crossed) > 0.0f) ? Vec3::normalize(crossed)
                                          : Vec3{0.0f, 0.0f, 1.0f};
  }

  /// State carried while turning faces into an indexed triangle list.
  struct MeshBuilder {
    /// The mesh being filled in.
    MeshData mesh;
    /// Shared vertices, keyed by position-and-normal.
    std::unordered_map<uint64_t, uint32_t> shared;
    /// Whether any vertex has been added yet (bounds seeding).
    bool has_bounds = false;
  };

  /// Grow the bounding box to include one position.
  void growBounds(MeshBuilder& builder, const Vec3& position) {
    if (!builder.has_bounds) {
      builder.mesh.min = position;
      builder.mesh.max = position;
      builder.has_bounds = true;
      return;
    }
    builder.mesh.min = {std::min(builder.mesh.min.x, position.x),
                        std::min(builder.mesh.min.y, position.y),
                        std::min(builder.mesh.min.z, position.z)};
    builder.mesh.max = {std::max(builder.mesh.max.x, position.x),
                        std::max(builder.mesh.max.y, position.y),
                        std::max(builder.mesh.max.z, position.z)};
  }

  /// Append a vertex and return its index, sharing when the corner names a
  /// normal and an identical vertex already exists.
  uint32_t addVertex(MeshBuilder& builder, const MeshVertex& vertex,
                     const ObjCorner& corner) {
    const bool shareable = corner.normal >= 0;
    if (shareable) {
      auto it = builder.shared.find(vertexKey(corner));
      if (it != builder.shared.end()) {
        return it->second;
      }
    }
    const auto index = static_cast<uint32_t>(builder.mesh.vertices.size());
    builder.mesh.vertices.push_back(vertex);
    growBounds(builder, vertex.position);
    if (shareable) {
      builder.shared.emplace(vertexKey(corner), index);
    }
    return index;
  }

  /// Position for a corner, or the origin when its index is out of range.
  Vec3 cornerPosition(const ObjSource& src, const ObjCorner& corner) {
    const bool valid =
        corner.position >= 0 &&
        static_cast<size_t>(corner.position) < src.positions.size();
    return valid ? src.positions[static_cast<size_t>(corner.position)] : Vec3{};
  }

  /// Normal for a corner, falling back to @p fallback when it declared none.
  Vec3 cornerNormal(const ObjSource& src, const ObjCorner& corner,
                    const Vec3& fallback) {
    const bool valid = corner.normal >= 0 &&
                       static_cast<size_t>(corner.normal) < src.normals.size();
    return valid ? src.normals[static_cast<size_t>(corner.normal)] : fallback;
  }

  /// Corners of one triangle of a face, fanned from its first corner.
  struct Triangle {
    /// First corner (the fan hub).
    ObjCorner a;
    /// Second corner.
    ObjCorner b;
    /// Third corner.
    ObjCorner c;
  };

  /// Emit one triangle into the builder.
  void addTriangle(MeshBuilder& builder, const ObjSource& src,
                   const Triangle& tri) {
    const Vec3 pa = cornerPosition(src, tri.a);
    const Vec3 pb = cornerPosition(src, tri.b);
    const Vec3 pc = cornerPosition(src, tri.c);
    const Vec3 flat = faceNormal(pa, pb, pc);
    builder.mesh.indices.push_back(
        addVertex(builder, {pa, cornerNormal(src, tri.a, flat)}, tri.a));
    builder.mesh.indices.push_back(
        addVertex(builder, {pb, cornerNormal(src, tri.b, flat)}, tri.b));
    builder.mesh.indices.push_back(
        addVertex(builder, {pc, cornerNormal(src, tri.c, flat)}, tri.c));
  }

  /// Triangulate every face into the builder.
  void buildTriangles(MeshBuilder& builder, const ObjSource& src) {
    for (const auto& face : src.faces) {
      for (size_t i = 2; i < face.size(); ++i) {
        addTriangle(builder, src, {face[0], face[i - 1], face[i]});
      }
    }
  }

}  // namespace

std::optional<MeshData> parseObjMesh(std::string_view text) {
  const ObjSource src = readSource(text);
  MeshBuilder builder;
  buildTriangles(builder, src);
  if (builder.mesh.indices.empty()) {
    return std::nullopt;
  }
  return std::move(builder.mesh);
}

std::optional<MeshData> loadObjMesh(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return std::nullopt;
  }
  std::ostringstream contents;
  contents << file.rdbuf();
  return parseObjMesh(contents.str());
}

}  // namespace eng
