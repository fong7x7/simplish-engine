#pragma once

/// @file footstep-names.h
/// @brief The words step sets and surfaces are written as, and read back.
/// @par Threading
/// Pure functions.

#include <game/content/footstep-surface.h>
#include <game/content/step-set.h>
#include <optional>
#include <string_view>

namespace eng::game {

/// The word @p steps is written as: `boots`.
[[nodiscard]] std::string_view stepSetWord(StepSet steps);

/// What a choice row calls @p steps: `Boots`.
[[nodiscard]] std::string_view stepSetLabel(StepSet steps);

/// The step set @p word names, or nothing.
[[nodiscard]] std::optional<StepSet> stepSetNamed(std::string_view word);

/// The word @p surface is written as: `wood`.
[[nodiscard]] std::string_view footstepSurfaceWord(FootstepSurface surface);

/// What a choice row calls @p surface: `Wood`.
[[nodiscard]] std::string_view footstepSurfaceLabel(FootstepSurface surface);

/// The surface @p word names, or nothing.
[[nodiscard]] std::optional<FootstepSurface>
footstepSurfaceNamed(std::string_view word);

}  // namespace eng::game
