#include <algorithm>
#include <charconv>
#include <engine/gui/gui-theme.h>
#include <engine/gui/gui-widget-tree.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <ranges>
#include <sstream>

namespace eng {

/// Fallback colour for missing tokens (magenta).
constexpr uint32_t FALLBACK_MAGENTA = 0xFF00FFFF;

namespace {

  /// Convert GuiWidgetType to its string key for overrides lookup.
  std::string_view widgetTypeKey(GuiWidgetType type) {
    switch (type) {
      case GuiWidgetType::PANEL:
        return "Panel";
      case GuiWidgetType::TEXT:
        return "Text";
      case GuiWidgetType::BUTTON:
        return "Button";
      case GuiWidgetType::TEXT_INPUT:
        return "TextInput";
      case GuiWidgetType::TEXT_AREA:
        return "TextArea";
      case GuiWidgetType::SCROLL_CONTAINER:
        return "ScrollContainer";
      case GuiWidgetType::IMAGE:
        return "Image";
      case GuiWidgetType::CUSTOM:
        return "Custom";
    }
    return "Panel";
  }

  /// Try to find a token in widget overrides of a theme.
  const TokenValue* findOverride(const Theme& theme, GuiWidgetType type,
                                 std::string_view token) {
    auto key = std::string(widgetTypeKey(type));
    auto ov_it = theme.widget_overrides.find(key);
    if (ov_it == theme.widget_overrides.end()) {
      return nullptr;
    }
    auto tok_it = ov_it->second.find(std::string(token));
    if (tok_it == ov_it->second.end()) {
      return nullptr;
    }
    return &tok_it->second;
  }

  /// Try to find a token in the base tokens of a theme.
  const TokenValue* findBaseToken(const Theme& theme, std::string_view token) {
    auto it = theme.tokens.find(std::string(token));
    return (it != theme.tokens.end()) ? &it->second : nullptr;
  }

  /// Search a single theme for override then base token.
  const TokenValue* findInTheme(const Theme& theme, GuiWidgetType type,
                                std::string_view token) {
    const auto* ov = findOverride(theme, type, token);
    return (ov != nullptr) ? ov : findBaseToken(theme, token);
  }

  /// Resolve a token from scoped themes (innermost first).
  const TokenValue* resolveFromScopes(const ThemeScopeStack& stack,
                                      GuiWidgetType type,
                                      std::string_view token) {
    for (const auto& scope : std::views::reverse(stack.scopes)) {
      if (scope.theme == nullptr) {
        continue;
      }
      const auto* val = findInTheme(*scope.theme, type, token);
      if (val != nullptr) {
        return val;
      }
    }
    return nullptr;
  }

  /// Resolve a token from the full stack including root theme.
  const TokenValue* resolveToken(const ThemeScopeStack& stack,
                                 GuiWidgetType type, std::string_view token) {
    const auto* val = resolveFromScopes(stack, type, token);
    if (val != nullptr) {
      return val;
    }
    if (stack.root_theme == nullptr) {
      return nullptr;
    }
    return findInTheme(*stack.root_theme, type, token);
  }

  /// Read entire file contents into a string.
  std::optional<std::string> readWholeFile(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) {
      return std::nullopt;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
  }

  /// Parse a hex colour string like "0x1E1E2EFF" to uint32_t.
  uint32_t parseHexColor(std::string_view s) {
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
      s.remove_prefix(2);
    }
    uint32_t result = 0;
    std::from_chars(s.data(), s.data() + s.size(), result, 16);
    return result;
  }

  /// Map a type string to TokenType enum value.
  std::optional<TokenType> parseTokenType(std::string_view s) {
    if (s == "color") {
      return TokenType::COLOR;
    }
    if (s == "float") {
      return TokenType::FLOAT;
    }
    if (s == "int") {
      return TokenType::INT;
    }
    if (s == "string") {
      return TokenType::STRING;
    }
    return std::nullopt;
  }

  void populateTokenValue(TokenValue& val, const nlohmann::json& obj) {
    switch (val.type) {
      case TokenType::COLOR:
        val.color = parseHexColor(obj.value("value", "0x0"));
        break;
      case TokenType::FLOAT:
        val.number = obj.value("value", 0.0f);
        break;
      case TokenType::INT:
        val.integer = obj.value("value", 0);
        break;
      case TokenType::STRING:
        val.str = obj.value("value", "");
        break;
    }
  }

  /// Parse a single token value from a JSON object.
  std::optional<TokenValue> parseTokenValue(const nlohmann::json& obj) {
    if (!obj.is_object() || !obj.contains("type")) {
      return std::nullopt;
    }
    auto token_type = parseTokenType(obj.value("type", ""));
    if (!token_type) {
      return std::nullopt;
    }
    TokenValue val;
    val.type = *token_type;
    populateTokenValue(val, obj);
    return val;
  }

  /// Parse a map of token name → TokenValue from a JSON object.
  std::unordered_map<std::string, TokenValue>
  parseTokenMap(const nlohmann::json& obj) {
    std::unordered_map<std::string, TokenValue> tokens;
    for (const auto& [key, value] : obj.items()) {
      auto tv = parseTokenValue(value);
      if (tv) {
        tokens[key] = std::move(*tv);
      }
    }
    return tokens;
  }

}  // namespace

std::optional<Theme> loadTheme(std::string_view json_path) {
  const std::filesystem::path path(json_path);
  if (!std::filesystem::exists(path)) {
    return std::nullopt;
  }
  const auto contents = readWholeFile(path);
  if (!contents) {
    return std::nullopt;
  }
  auto root = nlohmann::json::parse(*contents, nullptr, false);
  if (root.is_discarded() || !root.is_object()) {
    return std::nullopt;
  }
  Theme theme;
  theme.name = root.value("name", path.stem().string());
  if (root.contains("tokens") && root["tokens"].is_object()) {
    theme.tokens = parseTokenMap(root["tokens"]);
  }
  if (root.contains("widget_overrides") &&
      root["widget_overrides"].is_object()) {
    for (const auto& [key, value] : root["widget_overrides"].items()) {
      if (value.is_object()) {
        theme.widget_overrides[key] = parseTokenMap(value);
      }
    }
  }
  return theme;
}

void ThemeScopeStack::pushScope(const Theme* theme, GuiWidgetId root_widget) {
  scopes.push_back({theme, root_widget});
}

void ThemeScopeStack::popScope() {
  if (!scopes.empty()) {
    scopes.pop_back();
  }
}

uint32_t ThemeScopeStack::resolveColor(GuiWidgetType type,
                                       std::string_view token) const {
  const auto* val = resolveToken(*this, type, token);
  return (val != nullptr) ? val->color : FALLBACK_MAGENTA;
}

float ThemeScopeStack::resolveFloat(GuiWidgetType type,
                                    std::string_view token) const {
  const auto* val = resolveToken(*this, type, token);
  return (val != nullptr) ? val->number : 0.0f;
}

int32_t ThemeScopeStack::resolveInt(GuiWidgetType type,
                                    std::string_view token) const {
  const auto* val = resolveToken(*this, type, token);
  return (val != nullptr) ? val->integer : 0;
}

std::string_view ThemeScopeStack::resolveString(GuiWidgetType type,
                                                std::string_view token) const {
  const auto* val = resolveToken(*this, type, token);
  return (val != nullptr) ? std::string_view(val->str) : "";
}

}  // namespace eng
