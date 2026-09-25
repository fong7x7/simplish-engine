#include <algorithm>
#include <charconv>
#include <editor/build/editor-build-log.h>
#include <optional>
#include <string_view>

namespace eng::editor {

namespace {

  /// Where a diagnostic's words begin, and how bad it is.
  struct Marker {
    /// What separates where from what: `: error: `.
    std::string_view text;
    /// How bad a line with it is.
    EditorDiagnosticSeverity severity;
  };

  /// clang's and GCC's markers, then MSVC's, most specific first.
  constexpr Marker MARKERS[] = {
      {": fatal error: ", EditorDiagnosticSeverity::ERROR},
      {": error: ", EditorDiagnosticSeverity::ERROR},
      {": warning: ", EditorDiagnosticSeverity::WARNING},
      {"): error ", EditorDiagnosticSeverity::ERROR},
      {"): warning ", EditorDiagnosticSeverity::WARNING},
  };

  /// @p text as a number, or nothing.
  std::optional<uint32_t> number(std::string_view text) {
    uint32_t value = 0;
    const auto [end, ec] =
        std::from_chars(text.data(), text.data() + text.size(), value);
    return ec == std::errc{} && end == text.data() + text.size()
               ? std::optional{value}
               : std::nullopt;
  }

  /// Take the number after the last of @p separators off the end of
  /// @p where, if there is one there.
  uint32_t takeNumber(std::string_view& where, std::string_view separators) {
    const size_t at = where.find_last_of(separators);
    const auto value =
        at == std::string_view::npos ? std::nullopt : number(where.substr(at + 1));
    if (value) {
      where = where.substr(0, at);
    }
    return value.value_or(0);
  }

  /// @p where — `file:line:col` or `file(line,col` — into @p diagnostic's
  /// file, line and column. A `where` with no line is a tool, not a file:
  /// it goes in front of the message.
  void locate(std::string_view where, EditorBuildDiagnostic& diagnostic) {
    const uint32_t column = takeNumber(where, ":,");
    const uint32_t line = takeNumber(where, ":(");
    diagnostic.line = line != 0 ? line : column;
    diagnostic.column = line != 0 ? column : 0;
    if (diagnostic.line == 0) {
      diagnostic.message = std::string(where) + ": " + diagnostic.message;
      return;
    }
    diagnostic.file = std::string(where);
  }

  /// @p line as a compiler diagnostic, if it is one.
  std::optional<EditorBuildDiagnostic> compilerDiagnostic(std::string_view line) {
    for (const Marker& marker : MARKERS) {
      const size_t at = line.find(marker.text);
      if (at != std::string_view::npos && at > 0) {
        EditorBuildDiagnostic diagnostic{
            {}, 0, 0, marker.severity,
            std::string(line.substr(at + marker.text.size()))};
        locate(line.substr(0, at), diagnostic);
        return diagnostic;
      }
    }
    return std::nullopt;
  }

  /// @p line as one of CMake's errors, or one the build's own steps write
  /// (`error: …`), or a link failure, if it is one.
  std::optional<EditorBuildDiagnostic> otherDiagnostic(std::string_view line) {
    constexpr std::string_view CMAKE = "CMake Error at ";
    if (line.starts_with(CMAKE)) {
      EditorBuildDiagnostic diagnostic{{}, 0, 0, EditorDiagnosticSeverity::ERROR,
                                       "CMake Error"};
      std::string_view where = line.substr(CMAKE.size());
      where = where.substr(0, where.find(' '));
      locate(where, diagnostic);
      return diagnostic;
    }
    if (line.starts_with("error: ") || line.find("undefined reference") !=
                                           std::string_view::npos) {
      return EditorBuildDiagnostic{
          {}, 0, 0, EditorDiagnosticSeverity::ERROR,
          std::string(line.starts_with("error: ") ? line.substr(7) : line)};
    }
    return std::nullopt;
  }

  /// Whether @p a and @p b say the same thing about the same place.
  bool same(const EditorBuildDiagnostic& a, const EditorBuildDiagnostic& b) {
    return a.file == b.file && a.line == b.line && a.column == b.column &&
           a.message == b.message;
  }

}  // namespace

std::vector<EditorBuildDiagnostic>
buildDiagnostics(std::span<const std::string> lines, size_t count) {
  std::vector<EditorBuildDiagnostic> found;
  for (const std::string& line : lines) {
    auto diagnostic = compilerDiagnostic(line);
    if (!diagnostic) {
      diagnostic = otherDiagnostic(line);
    }
    if (diagnostic && found.size() < count &&
        std::ranges::none_of(found, [&](const EditorBuildDiagnostic& seen) {
          return same(seen, *diagnostic);
        })) {
      found.push_back(std::move(*diagnostic));
    }
  }
  return found;
}

}  // namespace eng::editor
