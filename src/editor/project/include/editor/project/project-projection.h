#pragma once

/// @file project-projection.h
/// @brief Which projection a project's world is drawn with.
/// @par Threading Thread-safe (pure functions over value types).

#include <cstdint>
#include <string_view>

namespace eng::editor {

/// The projection a project authors against.
///
/// A project setting rather than a build-time constant: the two look
/// different enough that art authored for one is wrong for the other, so it
/// belongs to the project the art lives in. Fixed for the life of a project
/// in practice — changing it invalidates any sprite already drawn against
/// it (see the ADR-003 amendments).
/// @thread_safety Immutable value type.
enum class ProjectProjection : uint8_t {
  /// Zero yaw: axis-aligned tiles, 64 wide by 48 deep, seen face-on.
  DIMETRIC,
  /// 45 degrees of yaw: 64 by 32 diamond tiles, the classic isometric look.
  ISOMETRIC,
};

/// Every projection, in enum order.
inline constexpr ProjectProjection PROJECT_PROJECTIONS[] = {
    ProjectProjection::DIMETRIC,
    ProjectProjection::ISOMETRIC,
};

/// The name this projection is written as in `project.json`.
[[nodiscard]] constexpr std::string_view
projectProjectionName(ProjectProjection projection) {
  return projection == ProjectProjection::ISOMETRIC ? "isometric" : "dimetric";
}

/// Read a projection back from a manifest.
///
/// Anything unrecognised — including the field being absent, which is every
/// project written before the setting existed — reads as dimetric, which is
/// what those projects were authored against.
[[nodiscard]] constexpr ProjectProjection
projectProjectionFromName(std::string_view name) {
  return name == "isometric" ? ProjectProjection::ISOMETRIC
                             : ProjectProjection::DIMETRIC;
}

}  // namespace eng::editor
