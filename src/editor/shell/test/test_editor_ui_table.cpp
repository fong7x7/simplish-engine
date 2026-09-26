#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <editor/project/project-text-file.h>
#include <editor/shell/editor-ui-table.h>
#include <filesystem>
#include <string>
#include <system_error>

using namespace eng;
using namespace eng::editor;

namespace {

/// A project folder with two screens and a broken one, removed after.
class UiProject {
public:
  UiProject()
    : root_(std::filesystem::temp_directory_path() /
            ("simplish-ui-table-" +
             // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
             std::to_string(reinterpret_cast<uintptr_t>(this)))) {
    write("pause", R"({"root": {"type": "panel", "children": [
        {"type": "button", "text": "Resume", "action": "resume"},
        {"type": "button", "text": "Quit", "action": "quit"}]}})");
    write("hud", R"({"layer": "hud", "anchor": "top_left",
        "root": {"type": "label", "text": "Score {score}"}})");
    write("Bad Name", R"({"root": {"type": "panel"}})");
    write("broken", "{");
  }
  ~UiProject() {
    std::error_code ec;
    std::filesystem::remove_all(root_, ec);
  }
  UiProject(const UiProject&) = delete;
  UiProject& operator=(const UiProject&) = delete;
  UiProject(UiProject&&) = delete;
  UiProject& operator=(UiProject&&) = delete;

  /// The project's folder.
  [[nodiscard]] const std::filesystem::path& root() const { return root_; }

private:
  /// Write the screen @p id as @p text.
  void write(const std::string& id, std::string_view text) {
    (void)writeProjectTextFile(editorUiScreenPath(root_, id), text);
  }

  /// The project's folder.
  std::filesystem::path root_;
};

}  // namespace

TEST_CASE("a project's screens load by name, and the rest say what is wrong") {
  const UiProject project;

  const EditorUiTable table = loadEditorUiTable(project.root());

  REQUIRE(table.screens.size() == 2);
  CHECK(table.screens[0].id == "hud");
  CHECK(table.screens[0].layer == game::UiScreenLayer::HUD);
  CHECK(findEditorUiScreen(table, "pause") != nullptr);
  REQUIRE(table.problems.size() == 2);
  CHECK(table.problems[0].starts_with("Bad Name.ui.json"));
  CHECK(table.problems[1].starts_with("broken.ui.json"));
}

TEST_CASE("a project's screens tell the game their ids and actions") {
  const UiProject project;
  game::GameContent content;

  addEditorUiContent(loadEditorUiTable(project.root()), content);

  CHECK(content.ui_screens == std::vector<std::string>{"hud", "pause"});
  CHECK(content.ui_actions == std::vector<std::string>{"quit", "resume"});
}

TEST_CASE("a screen's id is lowercase letters, digits, _ and -") {
  CHECK(editorUiScreenIdValid("game_over-2"));
  CHECK_FALSE(editorUiScreenIdValid("Pause"));
  CHECK_FALSE(editorUiScreenIdValid("../escape"));
  CHECK_FALSE(editorUiScreenIdValid(""));
}

TEST_CASE("a project's screens are drawn in its theme.json, when it reads") {
  const UiProject project;
  CHECK(loadEditorUiTable(project.root()).theme == nullptr);

  (void)writeProjectTextFile(editorUiThemePath(project.root()),
                             R"({"name": "Ember", "base": "light"})");
  const EditorUiTable themed = loadEditorUiTable(project.root());
  REQUIRE(themed.theme != nullptr);
  CHECK(themed.theme->name == "Ember");

  (void)writeProjectTextFile(editorUiThemePath(project.root()),
                             R"({"palette": {"primray": "#fff"}})");
  const EditorUiTable broken = loadEditorUiTable(project.root());
  CHECK(broken.theme == nullptr);
  CHECK(broken.problems.front().starts_with("theme.json: "));
}
