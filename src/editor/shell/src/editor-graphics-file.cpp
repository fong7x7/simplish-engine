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
  const json file{{WATER_KEY, waterFidelityWord(settings.water)}};
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
