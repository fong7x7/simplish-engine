#pragma once

/// @file fx-effect.h
/// @brief One effect: some bursts of particles and a flash of light.
/// @par Threading
/// A value type; the bursts it names must outlive it.

#include <engine/render-fx/fx-burst.h>
#include <engine/render-fx/fx-flash.h>
#include <engine/render-fx/fx-volume.h>
#include <span>

namespace eng {

/// What one moment of a fight looks like — a muzzle flash, a spark off a
/// wall, a blast: bursts of particles thrown out together, clouds of smoke
/// left standing, and a flash that lights the meshes around them. Any part
/// may be empty.
struct FxEffect {
  /// The bursts, each emitted from the same place and direction.
  std::span<const FxBurst> bursts{};
  /// The clouds of smoke left behind, each centred where it is emitted.
  std::span<const FxVolume> volumes{};
  /// The light; an intensity of zero is none.
  FxFlash flash{};
};

}  // namespace eng
