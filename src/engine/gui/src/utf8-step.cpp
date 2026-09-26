#include "utf8-step.h"

namespace eng {

namespace {

  constexpr uint32_t REPLACEMENT = 0xFFFDu;

  /// How many bytes a sequence starting with @p lead takes, and the bits
  /// of it that are the codepoint's; zero length for a stray byte.
  Utf8Step leadOf(uint8_t lead) {
    if (lead < 0x80U) {
      return {lead, 1};
    }
    if ((lead & 0xE0U) == 0xC0U) {
      return {lead & 0x1FU, 2};
    }
    if ((lead & 0xF0U) == 0xE0U) {
      return {lead & 0x0FU, 3};
    }
    if ((lead & 0xF8U) == 0xF0U) {
      return {lead & 0x07U, 4};
    }
    return {REPLACEMENT, 0};
  }

}  // namespace

Utf8Step decodeUtf8(std::string_view text, std::size_t at) {
  Utf8Step step = leadOf(static_cast<uint8_t>(text[at]));
  if (step.length == 0 || at + step.length > text.size()) {
    return {REPLACEMENT, 1};
  }
  for (std::size_t i = 1; i < step.length; ++i) {
    const auto byte = static_cast<uint8_t>(text[at + i]);
    if ((byte & 0xC0U) != 0x80U) {
      return {REPLACEMENT, 1};
    }
    step.codepoint = (step.codepoint << 6U) | (byte & 0x3FU);
  }
  return step;
}

}  // namespace eng
