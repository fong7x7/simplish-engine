#pragma once

/// @file logic-command.h
/// @brief One write of a project's game logic, waiting for its tick to end.
/// @par Threading
/// A value type.

#include <cstdint>
#include <game/logic/logic-target.h>
#include <game/world/logic-command-kind.h>

namespace eng::game {

/// A damage or a heal, queued by `GameLogicWorld` and applied in the order
/// queued once the logic's tick returns.
struct LogicCommand {
  /// What it does.
  LogicCommandKind kind = LogicCommandKind::DAMAGE;
  /// To whom.
  LogicTarget target{};
  /// How many health segments.
  uint16_t amount = 0;
};

}  // namespace eng::game
