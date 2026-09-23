#include <game/content/footstep-names.h>

namespace eng::game {

namespace {

  /// A word and a label, for one enumerator.
  struct Naming {
    /// As it is written in files and to the agent API.
    std::string_view word;
    /// As a choice row shows it.
    std::string_view label;
  };

  /// Every step set's naming, by `StepSet`.
  constexpr std::array<Naming, STEP_SET_COUNT> STEP_SET_NAMES{{
      {"default", "Default"},
      {"boots", "Boots"},
      {"bare", "Bare"},
      {"claws", "Claws"},
      {"heavy", "Heavy"},
  }};

  /// Every surface's naming, by `FootstepSurface`.
  constexpr std::array<Naming, FOOTSTEP_SURFACE_COUNT> SURFACE_NAMES{{
      {"ground", "Ground"},
      {"grass", "Grass"},
      {"dirt", "Dirt"},
      {"sand", "Sand"},
      {"water", "Water"},
      {"stone", "Stone"},
      {"wood", "Wood"},
      {"metal", "Metal"},
      {"cloth", "Cloth"},
  }};

  /// Where @p word sits in @p names, or nothing.
  template <size_t N>
  std::optional<size_t> indexOf(const std::array<Naming, N>& names,
                                std::string_view word) {
    for (size_t i = 0; i < N; ++i) {
      if (names[i].word == word) {
        return i;
      }
    }
    return std::nullopt;
  }

}  // namespace

std::string_view stepSetWord(StepSet steps) {
  return STEP_SET_NAMES[static_cast<size_t>(steps)].word;
}

std::string_view stepSetLabel(StepSet steps) {
  return STEP_SET_NAMES[static_cast<size_t>(steps)].label;
}

std::optional<StepSet> stepSetNamed(std::string_view word) {
  const std::optional<size_t> at = indexOf(STEP_SET_NAMES, word);
  return at ? std::optional{ALL_STEP_SETS[*at]} : std::nullopt;
}

std::string_view footstepSurfaceWord(FootstepSurface surface) {
  return SURFACE_NAMES[static_cast<size_t>(surface)].word;
}

std::string_view footstepSurfaceLabel(FootstepSurface surface) {
  return SURFACE_NAMES[static_cast<size_t>(surface)].label;
}

std::optional<FootstepSurface> footstepSurfaceNamed(std::string_view word) {
  const std::optional<size_t> at = indexOf(SURFACE_NAMES, word);
  return at ? std::optional{ALL_FOOTSTEP_SURFACES[*at]} : std::nullopt;
}

}  // namespace eng::game
