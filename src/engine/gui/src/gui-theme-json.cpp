#include <algorithm>
#include <array>
#include <engine/gui/gui-theme-json.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <utility>

namespace eng {

namespace {

  using nlohmann::json;

  /// Each palette key a theme file may set, and the colour it sets.
  constexpr std::array<std::pair<std::string_view, GuiColor GuiPalette::*>, 24>
      PALETTE_KEYS{{{"background", &GuiPalette::background},
                    {"surface", &GuiPalette::surface},
                    {"surface_raised", &GuiPalette::surface_raised},
                    {"surface_sunken", &GuiPalette::surface_sunken},
                    {"control", &GuiPalette::control},
                    {"control_hover", &GuiPalette::control_hover},
                    {"control_pressed", &GuiPalette::control_pressed},
                    {"border", &GuiPalette::border},
                    {"border_strong", &GuiPalette::border_strong},
                    {"text", &GuiPalette::text},
                    {"text_muted", &GuiPalette::text_muted},
                    {"text_disabled", &GuiPalette::text_disabled},
                    {"primary", &GuiPalette::primary},
                    {"primary_hover", &GuiPalette::primary_hover},
                    {"primary_pressed", &GuiPalette::primary_pressed},
                    {"on_primary", &GuiPalette::on_primary},
                    {"danger", &GuiPalette::danger},
                    {"danger_hover", &GuiPalette::danger_hover},
                    {"success", &GuiPalette::success},
                    {"warning", &GuiPalette::warning},
                    {"focus_ring", &GuiPalette::focus_ring},
                    {"selection", &GuiPalette::selection},
                    {"scrim", &GuiPalette::scrim},
                    {"shadow", &GuiPalette::shadow}}};

  /// The colour member @p key names, or null.
  GuiColor GuiPalette::* paletteMember(std::string_view key) {
    for (const auto& [name, member] : PALETTE_KEYS) {
      if (name == key) {
        return member;
      }
    }
    return nullptr;
  }

  /// Set palette colour @p key to @p value; false, with @p error, on an
  /// unknown key or a bad colour.
  bool readColor(const std::string& key, const json& value, GuiPalette& palette,
                 std::string& error) {
    GuiColor GuiPalette::* member = paletteMember(key);
    const auto color = value.is_string()
                           ? parseGuiColor(value.get<std::string>())
                           : std::nullopt;
    if (member == nullptr || !color) {
      error = "palette." + key + ": " +
              (member == nullptr ? "not a palette colour"
                                 : "not a colour, #rrggbb or #rrggbbaa");
      return false;
    }
    palette.*member = *color;
    return true;
  }

  /// Lay the file's `palette` over @p palette.
  bool readPalette(const json& file, GuiPalette& palette, std::string& error) {
    if (!file.contains("palette")) {
      return true;
    }
    for (const auto& [key, value] : file["palette"].items()) {
      if (!readColor(key, value, palette, error)) {
        return false;
      }
    }
    return true;
  }

  /// Whether @p values is an array of at most @p most numbers.
  bool isNumberList(const json& values, size_t most) {
    return values.is_array() && values.size() <= most &&
           std::ranges::all_of(
               values, [](const json& value) { return value.is_number(); });
  }

  /// Lay the file's array @p key over @p scale, first entries first.
  template <size_t N>
  bool readScale(const json& file, std::string_view key,
                 std::array<float, N>& scale, std::string& error) {
    const std::string name(key);
    if (!file.contains(name)) {
      return true;
    }
    const json& values = file[name];
    if (!isNumberList(values, N)) {
      error =
          name + ": not an array of up to " + std::to_string(N) + " numbers";
      return false;
    }
    for (size_t i = 0; i < values.size(); ++i) {
      scale[i] = values[i].get<float>();
    }
    return true;
  }

  /// The preset the file's `base` names: `dark` unless it says `light`.
  std::optional<GuiTheme> baseTheme(const json& file, std::string& error) {
    const std::string base = file.value("base", std::string("dark"));
    if (base != "dark" && base != "light") {
      error = "base: \"" + base + R"(" is neither "dark" nor "light")";
      return std::nullopt;
    }
    return base == "light" ? GuiTheme::light() : GuiTheme::dark();
  }

  /// Lay every scale in @p file over @p theme.
  bool readScales(const json& file, GuiTheme& theme, std::string& error) {
    if (!readScale(file, "spacing", theme.spacing, error) ||
        !readScale(file, "radii", theme.radii, error) ||
        !readScale(file, "text_sizes", theme.text_sizes, error)) {
      return false;
    }
    if (file.contains("transition_ms") && file["transition_ms"].is_number()) {
      theme.transition_seconds = file["transition_ms"].get<float>() / 1000.0f;
    }
    return true;
  }

}  // namespace

std::optional<GuiTheme> parseGuiTheme(std::string_view text,
                                      std::string& error) {
  const json file = json::parse(text, nullptr, false);
  if (file.is_discarded() || !file.is_object()) {
    error = "not a JSON object";
    return std::nullopt;
  }
  std::optional<GuiTheme> theme = baseTheme(file, error);
  if (!theme || !readPalette(file, theme->palette, error) ||
      !readScales(file, *theme, error)) {
    return std::nullopt;
  }
  theme->name = file.value("name", theme->name);
  theme->deriveComponents();
  return theme;
}

std::optional<GuiTheme> loadGuiTheme(std::string_view path,
                                     std::string& error) {
  std::ifstream file{std::string(path)};
  if (!file) {
    error = "cannot open " + std::string(path);
    return std::nullopt;
  }
  std::stringstream text;
  text << file.rdbuf();
  return parseGuiTheme(text.str(), error);
}

}  // namespace eng
