#include <algorithm>
#include <editor/build/editor-build-log.h>
#include <fstream>
#include <string_view>

namespace eng::editor {

namespace {

  /// Whether @p line reads as an error from a compiler, a linker or CMake.
  /// `: error` rather than `error ` for MSVC's `file(3): error C2143`, so
  /// a compile command's `-Werror` is not taken for one.
  bool namesError(std::string_view line) {
    constexpr std::string_view MARKS[] = {"error:", ": error", "CMake Error",
                                          "undefined reference",
                                          "Undefined symbols"};
    return std::ranges::any_of(
        MARKS, [&](std::string_view mark) { return line.contains(mark); });
  }

}  // namespace

std::vector<std::string> readLogLines(const std::filesystem::path& path) {
  std::vector<std::string> lines;
  std::ifstream file(path);
  for (std::string line; std::getline(file, line);) {
    lines.push_back(std::move(line));
  }
  return lines;
}

std::vector<std::string> readLogTail(const std::filesystem::path& path,
                                     size_t count) {
  std::vector<std::string> lines = readLogLines(path);
  if (lines.size() > count) {
    lines.erase(lines.begin(),
                lines.end() - static_cast<std::ptrdiff_t>(count));
  }
  return lines;
}

std::vector<std::string> buildErrorLines(std::span<const std::string> lines,
                                         size_t count) {
  std::vector<std::string> errors;
  for (const std::string& line : lines) {
    if (errors.size() < count && namesError(line)) {
      errors.push_back(line);
    }
  }
  return errors;
}

}  // namespace eng::editor
