#pragma once

/// @file editor-graphics-settings.h
/// @brief The user's graphics settings, as the shell holds them.
/// @par Threading Main-thread only.

#include <cstdint>
#include <engine/render-water/water-effects.h>
#include <engine/render-water/water-fidelity.h>
#include <filesystem>

namespace eng::editor {

/// How much the user's machine is asked to draw, where that is kept, and a
/// count of changes, so whatever changes it — the View menu, an agent's
/// `set_water_fidelity` — only has to bump `revision` for the editor to
/// apply and save it. The user's, not the project's, as their volumes are:
/// a slow machine is slow in every project.
struct EditorGraphicsSettings {
  /// How painted water is drawn.
  WaterFidelity water = WATER_DEFAULT_FIDELITY;
  /// Which of the water's costlier effects are drawn, whatever the
  /// fidelity: reflections, refraction, contact foam, caustics.
  WaterEffects water_effects{};
  /// The file they are kept in; empty when there is nowhere to keep them.
  std::filesystem::path file;
  /// Bumped on every change; the editor applies and saves when it moves on
  /// from the revision it last wrote.
  uint64_t revision = 0;
};

}  // namespace eng::editor
