#pragma once

/// @file editor-event-source.h
/// @brief Where the events a clip plays came from.
/// @par Threading A value type.

#include <cstdint>

namespace eng::editor {

/// Where a clip's events came from, which is what an agent is told so it
/// knows whether writing some would replace anything.
enum class EditorEventSource : uint8_t {
  /// None: the clip lifts no foot, and the project gives it nothing.
  NONE,
  /// Found in the clip: a footstep wherever a foot comes down.
  DETECTED,
  /// The project's animation events table.
  AUTHORED,
};

}  // namespace eng::editor
