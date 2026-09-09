#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-project-title.h>

using namespace eng::editor;

namespace {

/// Shell state with a project open and nothing yet edited in it.
EditorShellState openProjectState() {
  EditorShellState state;
  state.project.loaded = true;
  state.project.metadata.name = "Transit Station";
  return state;
}

/// Place something, as a drop or an agent's `place_asset` does.
void placeSomething(EditorShellState& state) {
  performEditorAction(state.history, state.document,
                      {.kind = EditorActionKind::PLACE_ASSET, .index = 0});
}

}  // namespace

TEST_CASE("a project with nothing to save is named plainly") {
  const EditorShellState state = openProjectState();

  REQUIRE(editorProjectDisplayName(state) == "Transit Station");
  REQUIRE(editorProjectTitle(state) == "Simplish Editor — Transit Station");
}

TEST_CASE("an unwritten edit marks the name with an asterisk") {
  EditorShellState state = openProjectState();
  placeSomething(state);

  REQUIRE(editorProjectDisplayName(state) == "Transit Station *");
  REQUIRE(editorProjectTitle(state) == "Simplish Editor — Transit Station *");
}

TEST_CASE("saving takes the asterisk away again") {
  EditorShellState state = openProjectState();
  placeSomething(state);
  markEditorChangesSaved(state.history);

  REQUIRE(editorProjectDisplayName(state) == "Transit Station");
}

TEST_CASE("with no project open there is no name to mark") {
  EditorShellState state;
  // Even with edits in hand, which is a state nothing can reach: there is
  // no project to have edited. The name must still read as no project.
  placeSomething(state);

  REQUIRE(editorProjectDisplayName(state) == "No project");
  REQUIRE(editorProjectTitle(state) == "Simplish Editor");
}
