#include "support/test-gltf.h"

#include <cmath>
#include <cstring>
#include <string_view>

namespace eng::gltf::test {

namespace {

  /// glTF component type codes used here.
  constexpr int COMPONENT_U8 = 5121;
  constexpr int COMPONENT_U16 = 5123;
  constexpr int COMPONENT_F32 = 5126;

  /// Components per element of glTF type @p type.
  size_t componentsOf(std::string_view type) {
    if (type == "SCALAR") {
      return 1;
    }
    if (type == "VEC2") {
      return 2;
    }
    if (type == "VEC3") {
      return 3;
    }
    return type == "VEC4" ? 4 : 16;
  }

  /// Standard base64, with padding.
  std::string base64(const std::vector<uint8_t>& bytes) {
    static constexpr char ALPHABET[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (size_t i = 0; i < bytes.size(); i += 3) {
      const uint32_t b0 = bytes[i];
      const uint32_t b1 = i + 1 < bytes.size() ? bytes[i + 1] : 0;
      const uint32_t b2 = i + 2 < bytes.size() ? bytes[i + 2] : 0;
      const uint32_t word = (b0 << 16U) | (b1 << 8U) | b2;
      out += ALPHABET[(word >> 18U) & 63U];
      out += ALPHABET[(word >> 12U) & 63U];
      out += i + 1 < bytes.size() ? ALPHABET[(word >> 6U) & 63U] : '=';
      out += i + 2 < bytes.size() ? ALPHABET[word & 63U] : '=';
    }
    return out;
  }

  /// Append a little-endian word.
  void appendWord(std::vector<uint8_t>& out, uint32_t word) {
    for (uint32_t shift = 0; shift < 32U; shift += 8U) {
      out.push_back(static_cast<uint8_t>((word >> shift) & 0xFFU));
    }
  }

  /// Append one GLB chunk, padded to four bytes with @p pad.
  void appendChunk(std::vector<uint8_t>& out, std::vector<uint8_t> data,
                   uint32_t type, uint8_t pad) {
    while (data.size() % 4 != 0) {
      data.push_back(pad);
    }
    appendWord(out, static_cast<uint32_t>(data.size()));
    appendWord(out, type);
    out.insert(out.end(), data.begin(), data.end());
  }

}  // namespace

TestGltf::TestGltf() {
  doc_["asset"] = {{"version", "2.0"}};
  doc_["bufferViews"] = nlohmann::json::array();
  doc_["accessors"] = nlohmann::json::array();
}

size_t TestGltf::addView(const void* data, size_t bytes) {
  while (bin_.size() % 4 != 0) {
    bin_.push_back(0);
  }
  const size_t offset = bin_.size();
  const auto* first = static_cast<const uint8_t*>(data);
  bin_.insert(bin_.end(), first, first + bytes);
  doc_["bufferViews"].push_back(
      {{"buffer", 0}, {"byteOffset", offset}, {"byteLength", bytes}});
  return doc_["bufferViews"].size() - 1;
}

size_t TestGltf::addAccessor(size_t view, size_t count, const char* type) {
  doc_["accessors"].push_back(
      {{"bufferView", view}, {"count", count}, {"type", type}});
  return doc_["accessors"].size() - 1;
}

size_t TestGltf::addFloats(const std::vector<float>& values, const char* type) {
  const size_t view = addView(values.data(), values.size() * sizeof(float));
  const size_t a = addAccessor(view, values.size() / componentsOf(type), type);
  doc_["accessors"][a]["componentType"] = COMPONENT_F32;
  return a;
}

size_t TestGltf::addBytes(const std::vector<uint8_t>& values,
                          const char* type) {
  const size_t view = addView(values.data(), values.size());
  const size_t a = addAccessor(view, values.size() / componentsOf(type), type);
  doc_["accessors"][a]["componentType"] = COMPONENT_U8;
  return a;
}

size_t TestGltf::addShorts(const std::vector<uint16_t>& values,
                           const char* type) {
  const size_t view = addView(values.data(), values.size() * 2);
  const size_t a = addAccessor(view, values.size() / componentsOf(type), type);
  doc_["accessors"][a]["componentType"] = COMPONENT_U16;
  return a;
}

std::string TestGltf::gltfText() const {
  nlohmann::json out = doc_;
  out["buffers"] = {
      {{"byteLength", bin_.size()},
       {"uri", "data:application/octet-stream;base64," + base64(bin_)}}};
  return out.dump();
}

std::vector<uint8_t> TestGltf::glbBytes() const {
  nlohmann::json out = doc_;
  out["buffers"] = {{{"byteLength", bin_.size()}}};
  const std::string text = out.dump();
  std::vector<uint8_t> body;
  appendChunk(body, {text.begin(), text.end()}, 0x4E4F534A, ' ');
  appendChunk(body, bin_, 0x004E4942, 0);
  std::vector<uint8_t> glb;
  appendWord(glb, 0x46546C67);
  appendWord(glb, 2);
  appendWord(glb, static_cast<uint32_t>(12 + body.size()));
  glb.insert(glb.end(), body.begin(), body.end());
  return glb;
}

namespace {

  /// The arm's triangle: base at the shoulder, tip two up and one across
  /// hanging from the elbow.
  void addArmMesh(TestGltf& g) {
    const size_t position = g.addFloats({0, 0, 0, 0, 2, 0, 1, 2, 0}, "VEC3");
    const size_t normal = g.addFloats({0, 0, 1, 0, 0, 1, 0, 0, 1}, "VEC3");
    const size_t joints =
        g.addBytes({0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}, "VEC4");
    // The tip's weights sum to two, which the loader scales back to one.
    const size_t weights =
        g.addFloats({1, 0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0}, "VEC4");
    const size_t indices = g.addShorts({0, 1, 2}, "SCALAR");
    g.doc()["meshes"] = {{{"primitives",
                           {{{"attributes",
                              {{"POSITION", position},
                               {"NORMAL", normal},
                               {"JOINTS_0", joints},
                               {"WEIGHTS_0", weights}}},
                             {"indices", indices}}}}}};
  }

  /// The skin and the node hierarchy, deliberately out of order.
  void addArmNodes(TestGltf& g) {
    // Inverse binds: the shoulder bound at the origin, the elbow one up.
    std::vector<float> binds(32, 0.0f);
    for (size_t k = 0; k < 2; ++k) {
      for (size_t d = 0; d < 4; ++d) {
        binds[k * 16 + d * 5] = 1.0f;
      }
    }
    binds[16 + 13] = -1.0f;
    const size_t ibm = g.addFloats(binds, "MAT4");
    g.doc()["nodes"] = {{{"name", "elbow"}, {"translation", {0, 1, 0}}},
                        {{"name", "shoulder"}, {"children", {0}}},
                        {{"name", "Armature"}, {"children", {1}}},
                        {{"name", "arm"}, {"mesh", 0}, {"skin", 0}}};
    g.doc()["skins"] = {{{"joints", {1, 0}}, {"inverseBindMatrices", ibm}}};
  }

  /// The "swing" clip: the shoulder a quarter turn about Z over a second.
  void addArmClip(TestGltf& g) {
    const float half = std::sqrt(0.5f);
    const size_t times = g.addFloats({0.0f, 1.0f}, "SCALAR");
    const size_t turns = g.addFloats({0, 0, 0, 1, 0, 0, half, half}, "VEC4");
    g.doc()["animations"] = {
        {{"name", "swing"},
         {"samplers", {{{"input", times}, {"output", turns}}}},
         {"channels",
          {{{"sampler", 0},
            {"target", {{"node", 1}, {"path", "rotation"}}}}}}}};
  }

}  // namespace

TestGltf armDocument() {
  TestGltf g;
  addArmMesh(g);
  addArmNodes(g);
  addArmClip(g);
  return g;
}

}  // namespace eng::gltf::test
