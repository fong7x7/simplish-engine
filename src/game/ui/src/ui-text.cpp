#include <charconv>
#include <game/ui/ui-text.h>

namespace eng::game {

namespace {

  /// Append to @p out the value of the `{key}` that starts at @p open in
  /// @p pattern, and say where the text after it starts; the rest of the
  /// pattern as it is when there is no closing brace.
  size_t fillOne(std::string_view pattern, size_t open, const UiValues& values,
                 std::string& out) {
    const size_t close = pattern.find('}', open);
    if (close == std::string_view::npos) {
      out.append(pattern.substr(open));
      return pattern.size();
    }
    const auto found = values.find(pattern.substr(open + 1, close - open - 1));
    if (found != values.end()) {
      out.append(found->second);
    }
    return close + 1;
  }

  /// @p text read whole as a number, if it is one.
  std::optional<float> numberOf(std::string_view text) {
    float value = 0.0F;
    const auto [end, ec] =
        std::from_chars(text.data(), text.data() + text.size(), value);
    return ec == std::errc{} && end == text.data() + text.size() &&
                   !text.empty()
               ? std::optional(value)
               : std::nullopt;
  }

}  // namespace

std::string fillUiText(std::string_view pattern, const UiValues& values) {
  std::string out;
  size_t at = 0;
  while (at < pattern.size()) {
    const size_t open = pattern.find('{', at);
    out.append(pattern.substr(at, open - at));
    if (open == std::string_view::npos) {
      break;
    }
    if (pattern.substr(open, 2) == "{{") {
      out.push_back('{');
      at = open + 2;
    } else {
      at = fillOne(pattern, open, values, out);
    }
  }
  return out;
}

std::optional<float> uiNumber(const UiValues& values, std::string_view key) {
  const auto found = values.find(key);
  return found != values.end() ? numberOf(found->second) : numberOf(key);
}

std::optional<bool> uiFlag(const UiValues& values, std::string_view binding) {
  if (binding.empty()) {
    return std::nullopt;
  }
  const bool negated = binding.front() == '!';
  const auto found = values.find(negated ? binding.substr(1) : binding);
  const bool set = found != values.end() && !found->second.empty() &&
                   found->second != "0" && found->second != "false";
  return set != negated;
}

}  // namespace eng::game
