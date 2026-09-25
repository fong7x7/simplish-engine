#pragma once

/// @file logic-test.h
/// @brief A run of the game a project's logic test plays and checks.
/// @par Threading
/// Main-thread-only.

#include <concepts>
#include <cstdint>
#include <game/logic/game-logic-world.h>
#include <game/sdk/test-check.h>
#include <game/sdk/test-input.h>
#include <source_location>
#include <string_view>

namespace eng::game::sdk {

/// One run of a level with the project's logic, for a test to drive: hold a
/// player's controls, run ticks, read the world through the interface the
/// logic uses, and check what should be true. Implemented by whoever runs
/// the tests — `simplish-logic-check`, after every build — on a fresh world
/// and a fresh instance of the logic.
///
/// @code
///   SIMPLISH_LOGIC_TEST(first_wave_comes_at_once, "main") {
///     test.run(2);
///     test.expect(sdk::countActors(test.world(), {.id_prefix = "wave"}) > 0,
///                 "wave 1 spawned");
///   }
/// @endcode
///
/// Every player starts with nothing held: a test gives each the controls
/// it wants. Ticks run exactly as a playtest's, so a test is as
/// deterministic as the logic it tests.
class LogicTest {
public:
  virtual ~LogicTest() = default;

  /// Run @p ticks ticks, or until the run is over.
  virtual void run(uint64_t ticks) = 0;
  /// Have the player in input slot @p slot — player 1 is 0 — hold @p input
  /// from the next tick until told otherwise.
  virtual void hold(uint8_t slot, const TestInput& input) = 0;
  /// The world as the last tick left it; tick 0 before any has run. Its
  /// writes do nothing: a test drives the game only through its players.
  [[nodiscard]] virtual const GameLogicWorld& world() const = 0;
  /// Whether the logic has logged a line containing @p text.
  [[nodiscard]] virtual bool logged(std::string_view text) const = 0;
  /// Note @p check: a failed one fails the test, which runs on.
  virtual void record(const TestCheck& check) = 0;

  /// Check @p condition — anything that tests true or false: a failed one
  /// fails the test, naming @p what and the line it was checked on.
  template <std::convertible_to<bool> Condition>
  void expect(const Condition& condition, std::string_view what,
              std::source_location where = std::source_location::current()) {
    record(
        {static_cast<bool>(condition), what, where.file_name(), where.line()});
  }

  /// Run a tick at a time until @p done holds of the world, or @p max_ticks
  /// have run. Whether it held.
  template <typename Done> bool runUntil(Done done, uint64_t max_ticks) {
    for (uint64_t i = 0; i < max_ticks && !done(world()); ++i) {
      run(1);
    }
    return done(world());
  }

  LogicTest(const LogicTest&) = delete;
  LogicTest& operator=(const LogicTest&) = delete;
  LogicTest(LogicTest&&) = delete;
  LogicTest& operator=(LogicTest&&) = delete;

protected:
  LogicTest() = default;
};

}  // namespace eng::game::sdk
