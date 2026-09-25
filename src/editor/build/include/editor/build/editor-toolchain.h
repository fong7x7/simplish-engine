#pragma once

/// @file editor-toolchain.h
/// @brief The tools the editor builds a project's C++ with.
/// @par Threading Thread-safe (value type; `editorToolchain` reads the
/// environment and is main-thread-only).

#include <filesystem>
#include <string>

namespace eng::editor {

/// What the editor runs to build a project's game logic and to deploy its
/// game: the same CMake, generator and compiler that built the editor
/// itself, and the engine source tree that did.
///
/// The same compiler is not a convenience. A playtest calls the project's
/// logic through C++ virtual functions, and a shared library built by
/// another compiler — or another standard library — lays those tables out
/// differently. So the defaults are baked into the editor when it is built
/// rather than looked up on the `PATH`, which an editor started from a
/// desktop does not have anyway.
/// @thread_safety Immutable value type.
struct EditorToolchain {
  /// The `cmake` executable.
  std::string cmake;
  /// The CMake generator: `Ninja`, `Unix Makefiles`, ...
  std::string generator;
  /// The build tool that generator drives, or empty for CMake to find.
  std::string make_program;
  /// The C++ compiler.
  std::string cxx_compiler;
  /// The engine's source tree: where `cmake/SimplishGameLogic.cmake` and
  /// the headers a project's logic includes are.
  std::filesystem::path engine_root;
  /// `simplish-logic-check`, which runs a freshly built library in a
  /// process of its own before the editor loads it; empty when this
  /// editor was built without it.
  std::filesystem::path logic_check{};
};

/// The toolchain the editor was built with. `SIMPLISH_ENGINE_ROOT` in the
/// environment overrides the engine tree, for an editor moved away from
/// the checkout that built it.
[[nodiscard]] EditorToolchain editorToolchain();

}  // namespace eng::editor
