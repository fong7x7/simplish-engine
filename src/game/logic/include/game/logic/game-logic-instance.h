#pragma once

/// @file game-logic-instance.h
/// @brief One run's instance of a project's logic, and who unmakes it.
/// @par Threading
/// Main-thread-only.

#include <game/logic/game-logic-factory.h>
#include <game/logic/game-logic.h>

namespace eng::game {

/// Owns one instance of a project's logic, made by a factory and unmade by
/// the same factory's destroy when this goes. What whoever runs a game —
/// the editor's playtest, a deployed game — holds beside its `GameWorld`,
/// which only borrows the logic.
///
/// The module the factory came from must stay loaded for as long as this
/// lives: the instance's code is that module's.
class GameLogicInstance {
public:
  /// No logic.
  GameLogicInstance() = default;
  /// A fresh instance from @p factory; none when it is empty or its create
  /// made nothing.
  explicit GameLogicInstance(GameLogicFactory factory);
  ~GameLogicInstance();

  GameLogicInstance(const GameLogicInstance&) = delete;
  GameLogicInstance& operator=(const GameLogicInstance&) = delete;
  /// Takes @p other's instance, leaving it with none.
  GameLogicInstance(GameLogicInstance&& other) noexcept;
  /// Unmakes this one's instance and takes @p other's.
  GameLogicInstance& operator=(GameLogicInstance&& other) noexcept;

  /// The instance, or null when there is none.
  [[nodiscard]] GameLogic* get() const { return logic_; }

private:
  /// Unmake the instance, if there is one.
  void reset();

  /// The instance; null for none.
  GameLogic* logic_ = nullptr;
  /// What unmakes it.
  GameLogicDestroyFn destroy_ = nullptr;
};

}  // namespace eng::game
