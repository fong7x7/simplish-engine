#pragma once

/// @file project-text-file.h
/// @brief Reading and writing whole text files without exceptions.
/// @par Threading Main-thread-only (touches the filesystem).

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace eng::editor {

/// Read a whole file. Nothing when it cannot be opened or read.
///
/// Failure comes back in the return type rather than as a throw, because
/// this build has exceptions disabled
/// (docs/decisions/ADR-001-no-exceptions.md), and every filesystem call
/// below uses the `std::error_code` overload for the same reason.
[[nodiscard]] std::optional<std::string>
readProjectTextFile(const std::filesystem::path& path);

/// Write a whole file, creating the directories above it. False on any
/// filesystem failure, with nothing written.
bool writeProjectTextFile(const std::filesystem::path& path,
                          std::string_view contents);

}  // namespace eng::editor
