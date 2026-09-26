#include <editor/project/project-text-file.h>
#include <editor/shell/editor-graphics-file.h>
#include <engine/core/logger.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <system_error>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// The key the water's fidelity is kept under.
  constexpr const char* WATER_KEY = "water";

  /// The key the water's effects are kept under.
  constexpr const char* EFFECTS_KEY = "water_effects";

  /// The key the interface's scale is kept under, and the scales allowed.
  constexpr const char* INTERFACE_KEY = "interface_scale";
  constexpr float MIN_INTERFACE_SCALE = 0.5f;
  constexpr float MAX_INTERFACE_SCALE = 3.0f;

  /// The interface scale @p file keeps, or nothing — with why in
  /// @p problems — when it keeps none or one out of range.
  std::optional<float> readInterface(const json& file,
                                     std::vector<std::string>& problems) {
    if (!file.contains(INTERFACE_KEY)) {
      return std::nullopt;
    }
    const json& entry = file.at(INTERFACE_KEY);
    if (!entry.is_number() || entry.get<float>() < MIN_INTERFACE_SCALE ||
        entry.get<float>() > MAX_INTERFACE_SCALE) {
      problems.emplace_back("interface_scale must be a number from 0.5 to 3");
      return std::nullopt;
    }
    return entry.get<float>();
  }

  /// The key reduced motion is kept under.
  constexpr const char* MOTION_KEY = "reduce_motion";

  /// The motion @p file keeps, or nothing — with why in @p problems — when
  /// it keeps none or one that is not a boolean.
  std::optional<GuiMotion> readMotion(const json& file,
                                      std::vector<std::string>& problems) {
    if (!file.contains(MOTION_KEY)) {
      return std::nullopt;
    }
    if (!file.at(MOTION_KEY).is_boolean()) {
      problems.emplace_back("reduce_motion must be true or false");
      return std::nullopt;
    }
    return file.at(MOTION_KEY).get<bool>() ? GuiMotion::REDUCED
                                           : GuiMotion::FULL;
  }

  /// One effect's switch from @p entry, the value under its word, into
  /// @p effects; a line in @p problems when it is not a boolean.
  void readEffect(const json& entry, WaterEffect effect, WaterEffects& effects,
                  std::vector<std::string>& problems) {
    if (!entry.is_boolean()) {
      problems.emplace_back(std::string(EFFECTS_KEY) + "." +
                            std::string(waterEffectWord(effect)) +
                            " must be true or false");
      return;
    }
    effects.on[waterEffectIndex(effect)] = entry.get<bool>();
  }

  /// The water's effects @p file's entry switches, over all of them on,
  /// with why in @p problems for each it gets wrong.
  WaterEffects readEffects(const json& file,
                           std::vector<std::string>& problems) {
    WaterEffects effects{};
    const json entry = file.value(EFFECTS_KEY, json::object());
    if (!entry.is_object()) {
      problems.emplace_back(std::string(EFFECTS_KEY) + " must be an object");
      return effects;
    }
    for (const auto& [word, value] : entry.items()) {
      if (const std::optional<WaterEffect> effect = waterEffectNamed(word)) {
        readEffect(value, *effect, effects, problems);
      } else {
        problems.emplace_back(std::string(EFFECTS_KEY) + " has no " + word);
      }
    }
    return effects;
  }

  /// @p effects as the file holds them: every effect's word and switch.
  json effectsJson(const WaterEffects& effects) {
    json out = json::object();
    for (const WaterEffect effect : WATER_EFFECT_LIST) {
      out[std::string(waterEffectWord(effect))] =
          waterEffectOn(effects, effect);
    }
    return out;
  }

  /// The fidelity @p file's water entry names, or nothing — with why in
  /// @p problems — when it names none.
  std::optional<WaterFidelity> readWater(const json& file,
                                         std::vector<std::string>& problems) {
    if (!file.contains(WATER_KEY)) {
      return std::nullopt;
    }
    const json& entry = file.at(WATER_KEY);
    const std::optional<WaterFidelity> fidelity =
        entry.is_string() ? waterFidelityNamed(entry.get<std::string>())
                          : std::nullopt;
    if (!fidelity) {
      problems.emplace_back("water must be flat, low or high");
    }
    return fidelity;
  }

}  // namespace

EditorGraphicsSettings parseEditorGraphics(const std::string& text,
                                           std::vector<std::string>& problems) {
  EditorGraphicsSettings settings{};
  const json file = json::parse(text, nullptr, false);
  if (!file.is_object()) {
    problems.emplace_back("not a JSON object; using the defaults");
    return settings;
  }
  settings.water = readWater(file, problems).value_or(settings.water);
  settings.water_effects = readEffects(file, problems);
  settings.ui_scale = readInterface(file, problems).value_or(settings.ui_scale);
  settings.motion = readMotion(file, problems).value_or(settings.motion);
  return settings;
}

namespace {

  /// The settings @p text, read from @p file, holds, remembering the file,
  /// with whatever it got wrong logged.
  EditorGraphicsSettings parseLogged(const std::filesystem::path& file,
                                     const std::string& text) {
    std::vector<std::string> problems;
    EditorGraphicsSettings settings = parseEditorGraphics(text, problems);
    settings.file = file;
    for (const std::string& problem : problems) {
      LOG_WARN("editor", file.filename().string() + ": " + problem);
    }
    return settings;
  }

}  // namespace

std::string writeEditorGraphics(const EditorGraphicsSettings& settings) {
  const json file{{WATER_KEY, waterFidelityWord(settings.water)},
                  {EFFECTS_KEY, effectsJson(settings.water_effects)},
                  {INTERFACE_KEY, settings.ui_scale},
                  {MOTION_KEY, settings.motion == GuiMotion::REDUCED}};
  return file.dump(2) + "\n";
}

EditorGraphicsSettings loadEditorGraphics(const std::filesystem::path& file) {
  EditorGraphicsSettings settings{};
  settings.file = file;
  std::error_code error;
  if (file.empty() || !std::filesystem::exists(file, error)) {
    (void)saveEditorGraphics(settings);
    return settings;
  }
  if (const std::optional<std::string> text = readProjectTextFile(file)) {
    return parseLogged(file, *text);
  }
  LOG_WARN("editor", "Could not read " + file.string() + "; the defaults");
  return settings;
}

bool saveEditorGraphics(const EditorGraphicsSettings& settings) {
  if (settings.file.empty()) {
    return false;
  }
  if (!writeProjectTextFile(settings.file, writeEditorGraphics(settings))) {
    LOG_WARN("editor", "Could not write the graphics settings to " +
                           settings.file.string());
    return false;
  }
  return true;
}

}  // namespace eng::editor
