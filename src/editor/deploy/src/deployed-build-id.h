#pragma once

/// @file deployed-build-id.h
/// @brief What identifies the code a deployed game simulates with.
/// @par Threading Pure functions.

#include <cstdint>

namespace eng::editor {

/// This binary's build id for a co-op `NetHello`: a hash of the engine
/// revision it was built from. Built from the same commit, two binaries
/// agree on every platform; the project's own logic is told apart by the
/// deploy manifest's logic hash, which is part of the content.
[[nodiscard]] uint64_t deployedBuildId();

}  // namespace eng::editor
