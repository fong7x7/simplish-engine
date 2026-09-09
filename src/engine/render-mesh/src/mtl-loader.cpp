#include <engine/render-mesh/mtl-loader.h>
#include <vector>

namespace eng {

namespace {

  /// Split a line into its keyword and the rest, both trimmed.
  ///
  /// The value keeps its inner spaces: `map_Kd my texture.png` is a path
  /// with a space in it, and splitting it into tokens would lose the file.
  struct MtlStatement {
    /// The leading keyword, or empty for a blank or comment line.
    std::string_view keyword;
    /// Everything after it, trimmed at both ends.
    std::string_view value;
  };

  bool isSpace(char c) {
    return c == ' ' || c == '\t';
  }

  std::string_view trim(std::string_view text) {
    while (!text.empty() && isSpace(text.front())) {
      text.remove_prefix(1);
    }
    while (!text.empty() && isSpace(text.back())) {
      text.remove_suffix(1);
    }
    return text;
  }

  MtlStatement readStatement(std::string_view line) {
    const std::string_view trimmed = trim(line);
    if (trimmed.empty() || trimmed.front() == '#') {
      return {};
    }
    size_t end = 0;
    while (end < trimmed.size() && !isSpace(trimmed[end])) {
      ++end;
    }
    return {trimmed.substr(0, end), trim(trimmed.substr(end))};
  }

  /// The last token of a `map_Kd` value, which is the file name.
  ///
  /// The statement may carry options before the path — `map_Kd -s 1 1 1
  /// wood.png` is legal — and every one of them is a flag and its
  /// arguments, so the file is what is left at the end. A path with spaces
  /// in it and no options still works, since nothing before it is dropped
  /// unless a leading `-` says there were options.
  std::string_view mapPath(std::string_view value) {
    if (value.empty() || value.front() != '-') {
      return value;
    }
    const size_t last_space = value.find_last_of(" \t");
    if (last_space == std::string_view::npos) {
      return value;
    }
    return trim(value.substr(last_space + 1));
  }

  /// Whether the reader is inside the material it was asked for.
  ///
  /// An empty request matches every material, so a library with one
  /// material answers whether or not the OBJ named it.
  bool wanted(std::string_view current, std::string_view requested) {
    return requested.empty() || current == requested;
  }

  /// The next line of @p text from @p start, with its terminator and any
  /// carriage return removed. `start` is advanced past it.
  std::string_view nextLine(std::string_view text, size_t& start) {
    size_t end = text.find('\n', start);
    if (end == std::string_view::npos) {
      end = text.size();
    }
    std::string_view line = text.substr(start, end - start);
    start = end + 1;
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    return line;
  }

  /// The map path @p statement yields while reading @p material, or empty
  /// for every statement that is not the one being looked for. A statement
  /// opening a material moves @p current onto it.
  std::string_view readMapStatement(const MtlStatement& statement,
                                    std::string_view material,
                                    std::string_view& current) {
    if (statement.keyword == "newmtl") {
      current = statement.value;
      return {};
    }
    if (statement.keyword != "map_Kd" || !wanted(current, material)) {
      return {};
    }
    return mapPath(statement.value);
  }

}  // namespace

std::optional<std::string> parseMtlDiffuseMap(std::string_view text,
                                              std::string_view material) {
  std::string_view current;
  size_t start = 0;
  while (start <= text.size()) {
    const std::string_view path = readMapStatement(
        readStatement(nextLine(text, start)), material, current);
    if (!path.empty()) {
      return std::string(path);
    }
  }
  return std::nullopt;
}

}  // namespace eng
