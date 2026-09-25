#pragma once

/// @file editor-build-log.h
/// @brief Reading back what a build printed.
/// @par Threading Thread-safe (pure functions, and a read of a file).

#include <cstddef>
#include <editor/build/editor-build-diagnostic.h>
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

/// Of @p lines, the first @p count errors and warnings the way clang, GCC,
/// MSVC, CMake and the build's own steps write them, taken apart into file,
/// line, column and message. Notes are left out, and a diagnostic repeated
/// word for word is kept once.
[[nodiscard]] std::vector<EditorBuildDiagnostic>
buildDiagnostics(std::span<const std::string> lines, size_t count);

/// Of @p lines, the first @p count that name an error the way compilers,
/// linkers and CMake do — `error:`, `Error`, `undefined reference`.
[[nodiscard]] std::vector<std::string>
buildErrorLines(std::span<const std::string> lines, size_t count);

}  // namespace eng::editor
