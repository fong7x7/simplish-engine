#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-menu-availability.h>

using namespace eng::editor;

namespace {

EditorShellState withProject() {
  EditorShellState state;
  state.project.loaded = true;
  return state;
}

}  // namespace

TEST_CASE("the Build rows need a project") {
  const EditorShellState none;

  CHECK_FALSE(
      editorMenuCommandEnabled(none, EditorMenuCommand::BUILD_GAME_LOGIC));
  CHECK_FALSE(editorMenuCommandEnabled(none, EditorMenuCommand::DEPLOY_GAME));
  CHECK(editorMenuCommandEnabled(withProject(),
                                 EditorMenuCommand::BUILD_GAME_LOGIC));
}

TEST_CASE("one build runs at a time") {
  EditorShellState state = withProject();
  state.build.status = EditorBuildStatus::RUNNING;

  CHECK_FALSE(
      editorMenuCommandEnabled(state, EditorMenuCommand::BUILD_GAME_LOGIC));
  CHECK_FALSE(editorMenuCommandEnabled(state, EditorMenuCommand::DEPLOY_GAME));
}

TEST_CASE("New Game Logic is for a project with none") {
  EditorShellState state = withProject();
  CHECK(editorMenuCommandEnabled(state, EditorMenuCommand::NEW_GAME_LOGIC));

  state.build.has_logic = true;

  CHECK_FALSE(
      editorMenuCommandEnabled(state, EditorMenuCommand::NEW_GAME_LOGIC));
}
