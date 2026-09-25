#pragma once

/// @file water-splash.h
/// @brief What water throws up when something lands in it.
/// @par Threading Thread-safe (constant data).

#include <engine/render-fx/fx-effect.h>

namespace eng {

/// The spray a splash throws: droplets flung up and falling back as short
/// streaks, a lower ring of them thrown wide, and a wisp of mist, all lit
/// by the scene's lights. Played through the effects (`playFxEffect`),
/// pointing up and scaled to what landed: a shot at 1, a wading step
/// smaller, a blast larger. Presentation only, like the ripples it rides
/// on.
[[nodiscard]] const FxEffect& waterSplashEffect();

}  // namespace eng
