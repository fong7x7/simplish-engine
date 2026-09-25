#pragma once

/// @file editor-build-log.h
/// @brief Reading back what a build printed.
/// @par Threading Thread-safe (pure functions, and a read of a file).

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace eng::editor {

/// The last @p count lines of the file at @p path; none when it cannot be
/// read.
[[nodiscard]] std::vector<std::string>
readLogTail(const std::filesystem::path& path, size_t count);

/// Every line of the file at @p path; none when it cannot be read.
[[nodiscard]] std::vector<std::string>
readLogLines(const std::filesystem::path& path);

/// Of @p lines, the first @p count that name an error the way compilers,
/// linkers and CMake do — `error:`, `Error`, `undefined reference`.
[[nodiscard]] std::vector<std::string>
buildErrorLines(std::span<const std::string> lines, size_t count);

}  // namespace eng::editor
