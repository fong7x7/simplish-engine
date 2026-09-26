#include <algorithm>
#include <editor/project/project-paths.h>
#include <editor/project/project-text-file.h>
#include <editor/shell/editor-ui-table.h>
#include <engine/gui/gui-theme-json.h>
#include <game/ui/ui-actions.h>
#include <game/ui/ui-screen-json.h>
#include <nlohmann/json.hpp>

namespace eng::editor {

namespace {

  /// The screen id of the file @p path, when its name ends as a screen's.
  std::optional<std::string> screenIdOf(const std::filesystem::path& path) {
    const std::string name = path.filename().string();
    if (!name.ends_with(EDITOR_UI_FILE_SUFFIX)) {
      return std::nullopt;
    }
    return name.substr(0, name.size() - EDITOR_UI_FILE_SUFFIX.size());
  }

  /// Every screen file in @p dir, by id, sorted.
  std::vector<std::pair<std::string, std::filesystem::path>>
  screenFiles(const std::filesystem::path& dir) {
    std::vector<std::pair<std::string, std::filesystem::path>> files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
      if (const auto id = screenIdOf(entry.path());
          id && entry.is_regular_file()) {
        files.emplace_back(*id, entry.path());
      }
    }
    std::ranges::sort(files);
    return files;
  }

  /// Read the screen @p id from @p path into @p table.
  void readScreen(const std::string& id, const std::filesystem::path& path,
                  EditorUiTable& table) {
    const std::string file = id + std::string(EDITOR_UI_FILE_SUFFIX);
    const std::optional<std::string> text = readProjectTextFile(path);
    game::UiScreenRead read =
        text ? game::parseUiScreen(*text, id)
             : game::UiScreenRead{std::nullopt, {"could not be read"}};
    for (const std::string& problem : read.problems) {
      table.problems.push_back(file + ": " + problem);
    }
    if (read.screen) {
      table.screens.push_back(std::move(*read.screen));
    }
  }

  /// Read the theme from @p path, when there is one, into @p table.
  void readTheme(const std::filesystem::path& path, EditorUiTable& table) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
      return;
    }
    const std::optional<std::string> text = readProjectTextFile(path);
    std::string error = text ? "" : "could not be read";
    std::optional<GuiTheme> theme =
        text ? parseGuiTheme(*text, error) : std::nullopt;
    if (theme) {
      table.theme = std::make_shared<GuiTheme>(std::move(*theme));
    } else {
      table.problems.push_back(std::string(EDITOR_UI_THEME_FILE) + ": " +
                               error);
    }
  }

}  // namespace

std::filesystem::path editorUiThemePath(const std::filesystem::path& root) {
  return editorUiDirPath(root) / EDITOR_UI_THEME_FILE;
}

std::filesystem::path editorUiDirPath(const std::filesystem::path& root) {
  return projectContentPath(root) / EDITOR_UI_DIR_NAME;
}

std::filesystem::path editorUiScreenPath(const std::filesystem::path& root,
                                         std::string_view id) {
  return editorUiDirPath(root) /
         (std::string(id) + std::string(EDITOR_UI_FILE_SUFFIX));
}

bool editorUiScreenIdValid(std::string_view id) {
  return !id.empty() && id.size() <= 64 && std::ranges::all_of(id, [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
           c == '-';
  });
}

EditorUiTable loadEditorUiTable(const std::filesystem::path& root) {
  EditorUiTable table;
  readTheme(editorUiThemePath(root), table);
  for (const auto& [id, path] : screenFiles(editorUiDirPath(root))) {
    if (editorUiScreenIdValid(id)) {
      readScreen(id, path, table);
    } else {
      table.problems.push_back(path.filename().string() +
                               ": a screen's name is lowercase letters, "
                               "digits, _ and -");
    }
  }
  return table;
}

const game::UiScreen* findEditorUiScreen(const EditorUiTable& table,
                                         std::string_view id) {
  const auto found = std::ranges::find(table.screens, id, &game::UiScreen::id);
  return found != table.screens.end() ? &*found : nullptr;
}

game::UiValues editorUiValuesFromJson(std::string_view text) {
  game::UiValues values;
  const nlohmann::json object = nlohmann::json::parse(text, nullptr, false);
  if (object.is_discarded() || !object.is_object()) {
    return values;
  }
  for (const auto& [key, value] : object.items()) {
    if (value.is_string()) {
      values.emplace(key, value.get<std::string>());
    } else if (value.is_number()) {
      values.emplace(key, value.dump());
    }
  }
  return values;
}

void addEditorUiContent(const EditorUiTable& table,
                        game::GameContent& content) {
  content.ui_screens.clear();
  for (const game::UiScreen& screen : table.screens) {
    content.ui_screens.push_back(screen.id);
  }
  content.ui_actions = game::uiActions(table.screens);
}

}  // namespace eng::editor
