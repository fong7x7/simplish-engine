#pragma once

/// @file game-logic-entry.h
/// @brief The symbols a project's logic module exports, and the macro that
/// writes them.
/// @par Threading
/// Declarations only.

#include <cstdint>
#include <game/logic/game-logic.h>
#include <memory>
#include <string_view>

namespace eng::game {

/// The version of the game logic API these headers describe. A module
/// built against another version is refused when it is loaded, rather than
/// called through a table of functions laid out differently from the one
/// it was compiled for. Bump it in the same change as any edit to a header
/// under `game/logic/` that a module compiles against.
inline constexpr uint32_t GAME_LOGIC_API_VERSION = 1;

/// The exported name of the function reporting a module's API version.
inline constexpr std::string_view GAME_LOGIC_VERSION_SYMBOL =
    "simplishGameLogicApiVersion";
/// The exported name of the function making an instance.
inline constexpr std::string_view GAME_LOGIC_CREATE_SYMBOL =
    "simplishCreateGameLogic";
/// The exported name of the function unmaking one.
inline constexpr std::string_view GAME_LOGIC_DESTROY_SYMBOL =
    "simplishDestroyGameLogic";

}  // namespace eng::game

#ifdef _WIN32
#define SIMPLISH_GAME_LOGIC_EXPORT extern "C" __declspec(dllexport)
#else
#define SIMPLISH_GAME_LOGIC_EXPORT                                             \
  extern "C" __attribute__((visibility("default")))
#endif

/// Export @p LogicClass — a default-constructible `eng::game::GameLogic` —
/// as a project's game logic. Write it once, at namespace scope, in one
/// source file of the project's `src/`.
///
/// The same three functions serve both ways the logic reaches a game: the
/// editor finds them by name in the shared library it builds for a
/// playtest, and a deployed game links them in directly.
#define SIMPLISH_GAME_LOGIC(LogicClass)                                        \
  SIMPLISH_GAME_LOGIC_EXPORT uint32_t simplishGameLogicApiVersion() {          \
    return ::eng::game::GAME_LOGIC_API_VERSION;                                \
  }                                                                            \
  SIMPLISH_GAME_LOGIC_EXPORT ::eng::game::GameLogic*                           \
  simplishCreateGameLogic() {                                                  \
    return std::make_unique<LogicClass>().release();                           \
  }                                                                            \
  SIMPLISH_GAME_LOGIC_EXPORT void simplishDestroyGameLogic(                    \
      ::eng::game::GameLogic* logic) {                                         \
    const std::unique_ptr<::eng::game::GameLogic> owned(logic);                \
  }
