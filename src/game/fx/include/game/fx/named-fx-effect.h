#pragma once

/// @file named-fx-effect.h
/// @brief An effect by the name game logic's cues call it.
/// @par Threading
/// Pure; the effects it gives view tables fixed at build time.

#include <engine/render-fx/fx-effect.h>
#include <optional>
#include <string_view>

namespace eng::game {

/// The effect called @p id: a whole combat effect by its cue's name —
/// `combat.shot_fired`, `combat.shot_hit_body`, `combat.shot_hit_wall`,
/// `combat.blast`, the names its sounds go by — or a single preset's burst
/// and flash (`smoke`, `fireball`, `wall_sparks`, …); nothing for a name
/// that is neither.
[[nodiscard]] std::optional<FxEffect> findNamedFxEffect(std::string_view id);

}  // namespace eng::game
