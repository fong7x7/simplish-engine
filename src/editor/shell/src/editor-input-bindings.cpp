#include <editor/shell/editor-input-bindings.h>
#include <editor/shell/editor-playtest-controls.h>
#include <engine/client/desktop-key-names.h>
#include <engine/core/logger.h>
#include <engine/input/input-bindings-json.h>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

namespace eng::editor {

namespace {

  /// Everything in @p file, or nothing when it cannot be read.
  std::optional<std::string> readText(const std::filesystem::path& file) {
    std::ifstream in(file);
    if (!in) {
      return std::nullopt;
    }
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
  }

  /// The scheme @p text, read from @p file, describes over @p defaults,
  /// with whatever it got wrong logged.
  input::InputBindings parseScheme(const std::filesystem::path& file,
                                   const std::string& text,
                                   const input::InputBindings& defaults) {
    input::InputBindingsLoad load =
        input::parseInputBindings(text, defaults, client::desktopKeyNames());
    for (const std::string& problem : load.problems) {
      LOG_WARN("editor", file.filename().string() + ": " + problem);
    }
    return std::move(load.bindings);
  }

}  // namespace

input::InputBindings
loadEditorInputBindings(const std::filesystem::path& file) {
  input::InputBindings defaults = editorDefaultInputBindings();
  std::error_code error;
  if (file.empty()) {
    return defaults;
  }
  if (!std::filesystem::exists(file, error)) {
    (void)saveEditorInputBindings(file, defaults);
    return defaults;
  }
  if (const std::optional<std::string> text = readText(file)) {
    return parseScheme(file, *text, defaults);
  }
  LOG_WARN("editor", "Could not read " + file.string() + "; default controls");
  return defaults;
}

bool saveEditorInputBindings(const std::filesystem::path& file,
                             const input::InputBindings& bindings) {
  if (file.empty()) {
    return false;
  }
  std::error_code error;
  std::filesystem::create_directories(file.parent_path(), error);
  std::ofstream out(file);
  out << input::writeInputBindings(bindings, client::desktopKeyNames());
  if (!out) {
    LOG_WARN("editor",
             "Could not write the control scheme to " + file.string());
    return false;
  }
  return true;
}

}  // namespace eng::editor
