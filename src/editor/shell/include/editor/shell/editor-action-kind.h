#pragma once

/// @file editor-action-kind.h
/// @brief What one recorded editor operation did.
/// @par Threading Main-thread-only.

#include <cstdint>

namespace eng::editor {

/// Every mutating operation the editor records.
///
/// Undo has to cover all of them ([Editor REQUIREMENTS §3]), so an
/// operation that changes the document without a kind here is a bug: the
/// history would go on describing a document that has moved out from under
/// it. The switches over this enum carry no `default`, so adding a kind
/// fails the build until every one of them handles it.
/// @thread_safety Immutable value type.
enum class EditorActionKind : uint8_t {
  /// An asset was placed in the world.
  PLACE_ASSET,
};

}  // namespace eng::editor
