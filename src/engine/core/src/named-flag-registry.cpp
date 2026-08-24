#include <algorithm>
#include <engine/core/named-flag-registry.h>

namespace eng {

std::optional<uint8_t> NamedFlagRegistry::claim(std::string_view name) {
  if (next_bit_ >= MAX_FLAGS) {
    return std::nullopt;
  }
  // NOLINTNEXTLINE(modernize-use-ranges,llvm-use-ranges)
  auto it = std::find_if(entries_.begin(), entries_.end(),
                         [&](const FlagEntry& e) { return e.name == name; });
  if (it != entries_.end()) {
    return std::nullopt;
  }
  const uint8_t bit = next_bit_++;
  entries_.push_back({std::string(name), bit});
  return bit;
}

std::optional<uint8_t> NamedFlagRegistry::resolve(std::string_view name) const {
  // NOLINTNEXTLINE(modernize-use-ranges,llvm-use-ranges)
  auto it = std::find_if(entries_.begin(), entries_.end(),
                         [&](const FlagEntry& e) { return e.name == name; });
  if (it == entries_.end()) {
    return std::nullopt;
  }
  return it->bit_index;
}

uint8_t NamedFlagRegistry::count() const {
  return next_bit_;
}

}  // namespace eng
