#include "gltf-uri.h"

namespace eng::gltf {

namespace {

  /// What separates a data URI's header from its payload.
  constexpr std::string_view BASE64_MARKER = ";base64,";

  /// The six bits a base64 character stands for, or nothing for padding
  /// and characters outside the alphabet.
  std::optional<uint32_t> base64Value(char c) {
    if (c >= 'A' && c <= 'Z') {
      return static_cast<uint32_t>(c - 'A');
    }
    if (c >= 'a' && c <= 'z') {
      return static_cast<uint32_t>(c - 'a' + 26);
    }
    if (c >= '0' && c <= '9') {
      return static_cast<uint32_t>(c - '0' + 52);
    }
    if (c == '+' || c == '-') {
      return 62U;
    }
    if (c == '/' || c == '_') {
      return 63U;
    }
    return std::nullopt;
  }

  /// The value of one hexadecimal digit, or nothing.
  std::optional<int> hexValue(char c) {
    if (c >= '0' && c <= '9') {
      return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
      return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
      return c - 'A' + 10;
    }
    return std::nullopt;
  }

}  // namespace

bool isDataUri(std::string_view uri) {
  return uri.starts_with("data:");
}

// Algorithm: base64 decode — accumulate six bits per character, emit a byte
// whenever eight are waiting, and stop at the first '=' of the padding.
std::optional<std::vector<uint8_t>> decodeDataUri(std::string_view uri) {
  const size_t marker = uri.find(BASE64_MARKER);
  if (!isDataUri(uri) || marker == std::string_view::npos) {
    return std::nullopt;
  }
  std::vector<uint8_t> bytes;
  uint32_t bits = 0;
  uint32_t pending = 0;
  for (const char c : uri.substr(marker + BASE64_MARKER.size())) {
    if (c == '=') {
      break;
    }
    const auto value = base64Value(c);
    if (!value) {
      return std::nullopt;
    }
    bits = (bits << 6U) | *value;
    pending += 6U;
    if (pending >= 8U) {
      pending -= 8U;
      bytes.push_back(static_cast<uint8_t>((bits >> pending) & 0xFFU));
    }
  }
  return bytes;
}

std::string uriToPath(std::string_view uri) {
  std::string path;
  for (size_t i = 0; i < uri.size(); ++i) {
    const auto high = i + 2 < uri.size() ? hexValue(uri[i + 1]) : std::nullopt;
    const auto low = i + 2 < uri.size() ? hexValue(uri[i + 2]) : std::nullopt;
    if (uri[i] == '%' && high && low) {
      path.push_back(static_cast<char>(*high * 16 + *low));
      i += 2;
    } else {
      path.push_back(uri[i]);
    }
  }
  return path;
}

}  // namespace eng::gltf
