#pragma once

/// @file gltf-json.h
/// @brief Type-checked reads from a glTF document's JSON.
/// @par Threading
/// Pure functions.
///
/// Exceptions are disabled (ADR-001), and nlohmann's accessors abort on a
/// type mismatch rather than throw — so every read goes through these,
/// which check before they touch. A glTF file is outside input: a wrong
/// type in it is a load error, never a crash.

#include <cstddef>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace eng::gltf {

/// The document type every helper here reads.
using Json = nlohmann::json;

/// The member @p key of @p object, or null when @p object is not an object
/// or has no such member.
[[nodiscard]] const Json* jsonMember(const Json& object, const char* key);

/// Element @p index of the array @p key of @p object, or null.
[[nodiscard]] const Json* jsonElement(const Json& object, const char* key,
                                      size_t index);

/// Length of the array @p key of @p object; zero when there is none.
[[nodiscard]] size_t jsonArraySize(const Json& object, const char* key);

/// The non-negative integer @p key of @p object, or nothing.
[[nodiscard]] std::optional<size_t> jsonIndex(const Json& object,
                                              const char* key);

/// The non-negative integer at @p index of the array @p key, or nothing.
[[nodiscard]] std::optional<size_t> jsonIndexAt(const Json& object,
                                                const char* key, size_t index);

/// The string @p key of @p object, or empty.
[[nodiscard]] std::string jsonString(const Json& object, const char* key);

/// The boolean @p key of @p object, or false.
[[nodiscard]] bool jsonFlag(const Json& object, const char* key);

/// The number at @p index of the array @p key, or @p fallback when any
/// part of that is missing or is not a number.
[[nodiscard]] float jsonNumberAt(const Json& object, const char* key,
                                 size_t index, float fallback);

}  // namespace eng::gltf
