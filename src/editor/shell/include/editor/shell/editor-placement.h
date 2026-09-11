#pragma once

/// @file editor-placement.h
/// @brief One placed instance of an asset in the level.
/// @par Threading Main-thread-only.

#include <cstddef>
#include <editor/shell/iso-projection.h>
#include <engine/math/vec3.h>
#include <game/content/faction.h>
#include <string>

namespace eng::editor {

/// An asset placed at a world position.
///
/// Saved to the project's level file and read back when the project is
/// opened (`editor-level-io.h`), which is what makes the asset panel an
/// authoring tool rather than only a placement one.
/// @thread_safety Main-thread-only.
struct EditorPlacement {
  /// Stable identifier for this one placed thing: `crate_01`. Assigned
  /// when it is placed and never reused, so a logic file can say
  /// `prop:crate_01` and mean this crate rather than whatever currently
  /// sits at some position in a list — see `editor-entity-id.h`.
  std::string id{};
  /// Index into the shell's asset list. A handle for this session only:
  /// what survives a rescan is the asset's own id, which is what the
  /// index is rebound through.
  size_t asset = 0;
  /// World position of the placement's base.
  WorldPoint position{};
  /// Rotation about the placement's own origin, in degrees.
  ///
  /// Euler angles rather than a quaternion: this is what the properties
  /// panel shows and what a designer types, and the level format will
  /// serialise the same three numbers. The conversion to a matrix is one
  /// place (`makePlacementTransform`), which is where the axis order is
  /// defined.
  Vec3 rotation{};
  /// Uniform size multiplier on top of the one-tile fit, so 1 is the size a
  /// model is dropped at and 2 is twice that in every direction.
  ///
  /// One number rather than three. Every mesh shader carries the normal
  /// through the model matrix, which is only correct while the scale is the
  /// same on each axis; stretching one axis would shade the model wrong on
  /// four backends until each grew a normal matrix. Applied about the same
  /// point rotation is — the centre of the footprint at the height the model
  /// rests on — so a prop grows up and out from where it stands.
  float scale = 1.0f;
  /// Whether a player can walk through it. Solid by default: most of what
  /// is dropped into a level is a crate or a wall, and the few that are not
  /// — grass, a rug, a decal — are ticked off in the properties panel.
  bool collides = true;
  /// Name of the clip a rigged model plays, as its file names it: `walk`.
  ///
  /// Empty plays the model's first clip, which is what a model dropped in
  /// fresh does — so a character moves the moment it lands, and a model
  /// placed before its rig has loaded needs no clip written into it later.
  /// A name the model does not have is kept, not cleared, and plays the
  /// first clip too: a clip renamed in the source file should not silently
  /// rewrite the level. A static model has no clips and ignores this.
  std::string animation{};
  /// The behavior the prop runs in a playtest, by reference —
  /// `behavior:guard` — or empty for none.
  ///
  /// A prop with a behavior is an *actor*: in a playtest it perceives the
  /// players, plans paths and moves (`game::ActorPool`), drawn wherever the
  /// simulation has it rather than where it was placed. It is not a
  /// collision box for players while it does, whatever `collides` says —
  /// its body is the actor's. A reference the behaviors table no longer
  /// has is kept as written; the game runs such an actor as `idle`.
  std::string behavior{};
  /// Which side the prop is on when it has a behavior. Meaningless, and
  /// not saved, when it has none.
  game::Faction faction = game::Faction::HOSTILE;
};

}  // namespace eng::editor
