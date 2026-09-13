#pragma once

/// @file combat-fx-preset.h
/// @brief One of the bursts the combat effects are built from, by name.
/// @par Threading
/// A value type, and a table of them fixed at build time.

#include <engine/render-fx/fx-burst.h>
#include <engine/render-fx/fx-flash.h>
#include <span>
#include <string_view>

namespace eng::game {

/// A single burst of particles and the flash that goes with it, named, so
/// something outside combat — the editor's particle emitter — can start
/// from exactly what a shot or a blast throws and change it from there.
///
/// The presets are the combat effects taken apart: a blast's fireball,
/// embers and smoke are three presets, since an emitter throws one burst.
struct CombatFxPreset {
  /// What a level file and the agent API name it by: `wall_sparks`.
  std::string_view id;
  /// What the editor's Effect row shows: `Wall Sparks`.
  std::string_view name;
  /// Its particles.
  FxBurst burst;
  /// Its light; an intensity of zero is none.
  FxFlash flash;
};

/// Every preset, in the order the Effect row steps through them.
[[nodiscard]] std::span<const CombatFxPreset> combatFxPresets();

/// The preset called @p id, or null when none is.
[[nodiscard]] const CombatFxPreset* findCombatFxPreset(std::string_view id);

}  // namespace eng::game
