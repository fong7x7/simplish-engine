#pragma once

/// @file logic-command-kind.h
/// @brief What one write of a project's game logic does.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// The writes `GameLogicWorld` queues.
enum class LogicCommandKind : uint8_t {
  /// Take health.
  DAMAGE,
  /// Give health back.
  HEAL,
};

}  // namespace eng::game
