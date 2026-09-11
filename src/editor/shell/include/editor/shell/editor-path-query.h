#pragma once

/// @file editor-path-query.h
/// @brief Asking a level's navigation grid for a route.
/// @par Threading Main-thread-only.

#include <editor/shell/editor-navigation.h>
#include <editor/shell/editor-path-answer.h>
#include <engine/math/vec2.h>

namespace eng::editor {

/// Two points of a level, and how wide the walker between them is.
/// @thread_safety Value type.
struct EditorPathQuery {
  /// Where the walk starts, in tiles.
  Vec2 from{};
  /// Where it should end, in tiles.
  Vec2 to{};
  /// The walker's radius, in tiles.
  float radius = 0.3F;
};

/// The route an actor of @p query's radius would plan across @p navigation
/// from one point to the other: from and to the nearest cells it can stand
/// in, by the same A* and smoothing the game runs.
[[nodiscard]] EditorPathAnswer
findEditorPath(const EditorNavigation& navigation,
               const EditorPathQuery& query);

}  // namespace eng::editor
