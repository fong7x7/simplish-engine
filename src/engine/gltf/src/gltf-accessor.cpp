#include "gltf-accessor.h"

#include <algorithm>
#include <cstring>

namespace eng::gltf {

namespace {

  /// glTF's component type codes.
  constexpr uint32_t COMPONENT_I8 = 5120;
  constexpr uint32_t COMPONENT_U8 = 5121;
  constexpr uint32_t COMPONENT_I16 = 5122;
  constexpr uint32_t COMPONENT_U16 = 5123;
  constexpr uint32_t COMPONENT_U32 = 5125;
  constexpr uint32_t COMPONENT_F32 = 5126;

  /// Where one accessor's elements are, once every check has passed.
  struct AccessorWindow {
    /// First byte of element 0.
    const uint8_t* first = nullptr;
    /// Elements in the window.
    size_t count = 0;
    /// Bytes from one element to the next.
    size_t stride = 0;
    /// Components per element.
    size_t components = 0;
    /// glTF component type code.
    uint32_t component_type = 0;
    /// Whether integer components map onto [0, 1] or [-1, 1].
    bool normalized = false;
  };

  /// Bytes one component of @p type occupies, or zero for an unknown type.
  size_t componentBytes(uint32_t type) {
    switch (type) {
      case COMPONENT_I8:
      case COMPONENT_U8:
        return 1;
      case COMPONENT_I16:
      case COMPONENT_U16:
        return 2;
      case COMPONENT_U32:
      case COMPONENT_F32:
        return 4;
      default:
        return 0;
    }
  }

  /// Components an element of glTF type @p type has, or zero.
  size_t typeComponents(const std::string& type) {
    static constexpr std::pair<std::string_view, size_t> TYPES[] = {
        {"SCALAR", 1}, {"VEC2", 2}, {"VEC3", 3}, {"VEC4", 4}, {"MAT4", 16}};
    for (const auto& [name, count] : TYPES) {
      if (type == name) {
        return count;
      }
    }
    return 0;
  }

  /// Whether a window of @p w's shape, starting @p offset bytes into a
  /// view of @p view_length, stays inside it.
  bool windowFits(const AccessorWindow& w, size_t offset, size_t view_length) {
    const size_t element = w.components * componentBytes(w.component_type);
    if (w.count == 0) {
      return offset <= view_length;
    }
    const size_t last = offset + (w.count - 1) * w.stride + element;
    return w.stride >= element && last <= view_length && last >= offset;
  }

  /// The bytes buffer view @p view spans, or null when it lies outside its
  /// buffer. @p length receives its size.
  const uint8_t* viewBytes(const GltfDocument& document, const Json& view,
                           size_t& length) {
    const auto buffer = jsonIndex(view, "buffer");
    const size_t offset = jsonIndex(view, "byteOffset").value_or(0);
    length = jsonIndex(view, "byteLength").value_or(0);
    if (!buffer || *buffer >= document.buffers.size()) {
      return nullptr;
    }
    const auto& bytes = document.buffers[*buffer];
    if (offset > bytes.size() || length > bytes.size() - offset) {
      return nullptr;
    }
    return bytes.data() + offset;
  }

  /// The shape accessor @p entry declares, before its bytes are located.
  AccessorWindow declaredShape(const Json& entry) {
    AccessorWindow w;
    w.count = jsonIndex(entry, "count").value_or(0);
    w.components = typeComponents(jsonString(entry, "type"));
    w.component_type =
        static_cast<uint32_t>(jsonIndex(entry, "componentType").value_or(0));
    w.normalized = jsonFlag(entry, "normalized");
    return w;
  }

  /// The buffer view accessor @p entry reads through, or null.
  const Json* accessorView(const GltfDocument& document, const Json& entry) {
    const auto view = jsonIndex(entry, "bufferView");
    return view ? jsonElement(document.root, "bufferViews", *view) : nullptr;
  }

  /// Point @p w at its first element within @p view, checking the whole
  /// window lies inside the view and the view inside its buffer.
  bool placeWindow(const GltfDocument& document, const Json& view,
                   const Json& entry, AccessorWindow& w) {
    size_t view_length = 0;
    const uint8_t* bytes = viewBytes(document, view, view_length);
    const size_t offset = jsonIndex(entry, "byteOffset").value_or(0);
    w.stride = jsonIndex(view, "byteStride")
                   .value_or(w.components * componentBytes(w.component_type));
    if (bytes == nullptr || !windowFits(w, offset, view_length)) {
      return false;
    }
    w.first = bytes + offset;
    return true;
  }

  /// Locate accessor @p accessor's elements and check them against its
  /// buffer. Nullopt for anything this loader does not read.
  std::optional<AccessorWindow> locate(const GltfDocument& document,
                                       size_t accessor, size_t components) {
    const Json* entry = jsonElement(document.root, "accessors", accessor);
    if (entry == nullptr || jsonMember(*entry, "sparse") != nullptr) {
      return std::nullopt;
    }
    AccessorWindow w = declaredShape(*entry);
    const Json* view = accessorView(document, *entry);
    if (w.components != components || componentBytes(w.component_type) == 0 ||
        view == nullptr || !placeWindow(document, *view, *entry, w)) {
      return std::nullopt;
    }
    return w;
  }

  /// One integer component of type @p type at @p at, widened.
  template <typename T> T readAs(const uint8_t* at) {
    T value{};
    std::memcpy(&value, at, sizeof(T));
    return value;
  }

  /// One component at its stored value, whatever its type.
  float rawComponent(const uint8_t* at, uint32_t type) {
    if (type == COMPONENT_F32) {
      return readAs<float>(at);
    }
    if (type == COMPONENT_U8) {
      return static_cast<float>(at[0]);
    }
    if (type == COMPONENT_U16) {
      return static_cast<float>(readAs<uint16_t>(at));
    }
    if (type == COMPONENT_I8) {
      return static_cast<float>(readAs<int8_t>(at));
    }
    if (type == COMPONENT_I16) {
      return static_cast<float>(readAs<int16_t>(at));
    }
    return static_cast<float>(readAs<uint32_t>(at));
  }

  /// The stored value a normalised component of @p type reads as one.
  float normalizedRange(uint32_t type) {
    switch (type) {
      case COMPONENT_U8:
        return 255.0f;
      case COMPONENT_U16:
        return 65535.0f;
      case COMPONENT_I8:
        return 127.0f;
      case COMPONENT_I16:
        return 32767.0f;
      default:
        return 1.0f;
    }
  }

  /// One component as a float, normalised if @p w says so. Signed types
  /// clamp at -1, since their most negative value has no positive twin.
  float componentFloat(const AccessorWindow& w, const uint8_t* at) {
    const float raw = rawComponent(at, w.component_type);
    if (!w.normalized || w.component_type == COMPONENT_F32) {
      return raw;
    }
    return std::max(raw / normalizedRange(w.component_type), -1.0f);
  }

  /// One unsigned integer component of type @p type at @p at.
  uint32_t componentUint(const uint8_t* at, uint32_t type) {
    if (type == COMPONENT_U8) {
      return at[0];
    }
    if (type == COMPONENT_U16) {
      return readAs<uint16_t>(at);
    }
    return readAs<uint32_t>(at);
  }

  /// Address of component @p c of element @p i.
  const uint8_t* componentAt(const AccessorWindow& w, size_t i, size_t c) {
    return w.first + i * w.stride + c * componentBytes(w.component_type);
  }

}  // namespace

std::optional<std::vector<float>>
readAccessorFloats(const GltfDocument& document, size_t accessor,
                   size_t components) {
  const auto w = locate(document, accessor, components);
  if (!w) {
    return std::nullopt;
  }
  std::vector<float> out;
  out.reserve(w->count * components);
  for (size_t i = 0; i < w->count; ++i) {
    for (size_t c = 0; c < components; ++c) {
      out.push_back(componentFloat(*w, componentAt(*w, i, c)));
    }
  }
  return out;
}

std::optional<std::vector<uint32_t>>
readAccessorUints(const GltfDocument& document, size_t accessor,
                  size_t components) {
  const auto w = locate(document, accessor, components);
  const bool unsigned_int = w && (w->component_type == COMPONENT_U8 ||
                                  w->component_type == COMPONENT_U16 ||
                                  w->component_type == COMPONENT_U32);
  if (!unsigned_int) {
    return std::nullopt;
  }
  std::vector<uint32_t> out;
  out.reserve(w->count * components);
  for (size_t i = 0; i < w->count; ++i) {
    for (size_t c = 0; c < components; ++c) {
      out.push_back(componentUint(componentAt(*w, i, c), w->component_type));
    }
  }
  return out;
}

size_t accessorCount(const GltfDocument& document, size_t accessor) {
  const Json* entry = jsonElement(document.root, "accessors", accessor);
  return entry != nullptr ? jsonIndex(*entry, "count").value_or(0) : 0;
}

}  // namespace eng::gltf
