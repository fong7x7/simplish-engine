#pragma once

/// @file logic-command.h
/// @brief One write of a project's game logic, waiting for its tick to end.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/math/vec3.h>
#include <game/content/faction.h>
#include <game/logic/logic-target.h>
#include <game/world/logic-command-kind.h>

namespace eng::game {

/// One write to a player or an actor, queued by `GameLogicWorld` and
/// applied in the order queued once the logic's tick returns. Only the
/// fields its kind names are read.
struct LogicCommand {
  /// What it does.
  LogicCommandKind kind = LogicCommandKind::DAMAGE;
  /// To whom.
  LogicTarget target{};
  /// How many health segments, for `DAMAGE` and `HEAL`.
  uint16_t amount = 0;
  /// Where to, for `MOVE`.
  Vec3 at{};
  /// Which state of its behavior, by index, for `SET_STATE`.
  uint8_t state = 0;
  /// Which side, for `SET_FACTION`.
  Faction faction = Faction::HOSTILE;
};

}  // namespace eng::game
