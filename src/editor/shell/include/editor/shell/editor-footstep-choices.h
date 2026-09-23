#pragma once

/// @file editor-footstep-choices.h
/// @brief What the Surface and Footsteps rows list, and what each pick is.
/// @par Threading Thread-safe (pure functions).

#include <cstddef>
#include <game/content/footstep-surface.h>
#include <game/content/step-set.h>
#include <optional>
#include <string>
#include <vector>

namespace eng::editor {

/// What a prop's Surface row lists: "From the ground", which leaves the
/// ground under it heard, then every surface.
[[nodiscard]] std::vector<std::string> editorSurfaceChoiceNames();

/// Where @p surface sits in that list.
[[nodiscard]] size_t
editorSurfaceChoiceIndex(std::optional<game::FootstepSurface> surface);

/// The surface pick @p index of that list is — nothing for the first —
/// which only means anything for an index the list has.
[[nodiscard]] std::optional<game::FootstepSurface>
editorSurfaceChoice(size_t index);

/// What an actor's Footsteps row lists: every step set.
[[nodiscard]] std::vector<std::string> editorFootstepChoiceNames();

}  // namespace eng::editor
