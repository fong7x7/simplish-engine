#include "gltf-json.h"

namespace eng::gltf {

const Json* jsonMember(const Json& object, const char* key) {
  if (!object.is_object()) {
    return nullptr;
  }
  const auto found = object.find(key);
  return found == object.end() ? nullptr : &*found;
}

const Json* jsonElement(const Json& object, const char* key, size_t index) {
  const Json* array = jsonMember(object, key);
  if (array == nullptr || !array->is_array() || index >= array->size()) {
    return nullptr;
  }
  return &(*array)[index];
}

size_t jsonArraySize(const Json& object, const char* key) {
  const Json* array = jsonMember(object, key);
  return array != nullptr && array->is_array() ? array->size() : 0;
}

namespace {

  /// @p value as an index: a whole number no less than zero. Text parses
  /// such a number as unsigned, but one built in code from an `int` is
  /// signed, so both are accepted.
  std::optional<size_t> asIndex(const Json* value) {
    if (value == nullptr || !value->is_number_integer() ||
        value->get<int64_t>() < 0) {
      return std::nullopt;
    }
    return value->get<size_t>();
  }

}  // namespace

std::optional<size_t> jsonIndex(const Json& object, const char* key) {
  return asIndex(jsonMember(object, key));
}

std::optional<size_t> jsonIndexAt(const Json& object, const char* key,
                                  size_t index) {
  return asIndex(jsonElement(object, key, index));
}

std::string jsonString(const Json& object, const char* key) {
  const Json* value = jsonMember(object, key);
  return value != nullptr && value->is_string() ? value->get<std::string>()
                                                : std::string{};
}

bool jsonFlag(const Json& object, const char* key) {
  const Json* value = jsonMember(object, key);
  return value != nullptr && value->is_boolean() && value->get<bool>();
}

float jsonNumberAt(const Json& object, const char* key, size_t index,
                   float fallback) {
  const Json* value = jsonElement(object, key, index);
  return value != nullptr && value->is_number() ? value->get<float>()
                                                : fallback;
}

}  // namespace eng::gltf
