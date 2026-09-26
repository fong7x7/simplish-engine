#include <algorithm>
#include <editor/build/editor-build-commands.h>
#include <editor/build/editor-build-log.h>
#include <editor/build/editor-build-paths.h>
#include <editor/build/editor-deploy-manifest.h>
#include <editor/build/editor-logic-library-load.h>
#include <editor/build/editor-logic-source.h>
#include <editor/build/editor-setup-json.h>
#include <editor/build/editor-toolchain.h>
#include <editor/project/project-paths.h>
#include <editor/project/project-text-file.h>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-level-io.h>
#include <editor/shell/editor-level-json.h>
#include <editor/shell/editor-playtest-session.h>
#include <editor/shell/editor-ui-table.h>
#include <editor/shell/simplish-editor.h>
#include <engine/core/logger.h>
#include <system_error>

namespace eng::editor {

namespace {

  /// Where a deploy's content goes, inside the deployed game.
  std::filesystem::path deployContentPath(const std::filesystem::path& root) {
    return projectDeployPath(root) / DEPLOYED_CONTENT_DIR_NAME;
  }

  /// The level a deployed game starts on: `main` when the project has it,
  /// else the first level baked.
  std::string startLevel(const std::vector<std::string>& levels) {
    const bool has_main =
        std::ranges::find(levels, EDITOR_LEVEL_ID) != levels.end();
    return has_main || levels.empty() ? std::string(EDITOR_LEVEL_ID)
                                      : levels.front();
  }
  /// The manifest of a deploy of the project at @p root, called @p name,
  /// that baked @p levels: its logic, if any, identified by its sources.
  EditorDeployManifest deployManifest(std::string name,
                                      std::vector<std::string> levels,
                                      const std::filesystem::path& root) {
    EditorDeployManifest manifest{std::move(name),
                                  std::move(levels),
                                  {},
                                  projectHasLogic(root),
                                  projectLogicHash(root)};
    manifest.start_level = startLevel(manifest.levels);
    return manifest;
  }


  /// Copy the folder @p name of the project at @p root's content into the
  /// deployed game's content at @p content. False when it is there and
  /// will not copy.
  bool copyContentFolder(const std::filesystem::path& root,
                         const std::filesystem::path& content,
                         std::string_view name) {
    const std::filesystem::path from = projectContentPath(root) / name;
    std::error_code ec;
    if (!std::filesystem::exists(from, ec)) {
      return true;
    }
    const std::filesystem::path to = content / PROJECT_CONTENT_DIR_NAME / name;
    std::filesystem::create_directories(to, ec);
    std::filesystem::copy(from, to,
                          std::filesystem::copy_options::recursive |
                              std::filesystem::copy_options::overwrite_existing,
                          ec);
    return !ec;
  }

  /// Copy the project at @p root's data tables and screens into the
  /// deployed game's content at @p content. False when they will not copy.
  bool copyDataTables(const std::filesystem::path& root,
                      const std::filesystem::path& content) {
    return copyContentFolder(root, content, "data") &&
           copyContentFolder(root, content, EDITOR_UI_DIR_NAME);
  }

  /// What the status line says as a deploy of @p state's project starts —
  /// warning that the open level's unsaved edits are not in it: a deploy
  /// bakes the level files, not the document.
  std::string deployStartMessage(const EditorShellState& state) {
    const std::string unsaved =
        hasUnsavedEditorChanges(state.history)
            ? " (saved levels only — " + state.level_id + " has unsaved edits)"
            : "";
    return "Deploying" + unsaved +
           "; the first deploy builds the whole engine, and takes minutes";
  }

  /// What @p lines, a build's log, say of it into @p build: its last lines,
  /// its errors, and those taken apart. None for no lines.
  void readBuildLog(EditorBuildState& build,
                    const std::vector<std::string>& lines) {
    const size_t tail = std::min(lines.size(), EDITOR_BUILD_LOG_TAIL);
    build.log_tail.assign(lines.end() - static_cast<std::ptrdiff_t>(tail),
                          lines.end());
    build.errors = buildErrorLines(lines, EDITOR_BUILD_ERROR_LINES);
    build.diagnostics = buildDiagnostics(lines, EDITOR_BUILD_DIAGNOSTICS);
  }

  /// What the status line says about a logic build that ended as @p state.
  std::string logicBuildMessage(const EditorBuildState& state) {
    if (state.status != EditorBuildStatus::SUCCEEDED) {
      return "Game logic failed to build — " +
             (state.errors.empty() ? state.log.string() : state.errors.front());
    }
    return state.logic_error.empty()
               ? "Game logic built — the next playtest runs it"
               : "Game logic built, but would not load: " + state.logic_error;
  }

}  // namespace

bool SimplishEditor::runBuildCommand(EditorMenuCommand command) {
  if (command == EditorMenuCommand::NEW_GAME_LOGIC) {
    newGameLogic();
  } else if (command == EditorMenuCommand::BUILD_GAME_LOGIC) {
    buildGameLogic();
  } else if (command == EditorMenuCommand::DEPLOY_GAME) {
    deployGame();
  } else {
    return false;
  }
  return true;
}

void SimplishEditor::newGameLogic() {
  if (!state_.project.loaded) {
    return;
  }
  const EditorLogicScaffold made = scaffoldProjectLogic(state_.project.root);
  refreshLogicState();
  reloadUi();  // the scaffold's pause menu and HUD
  if (made == EditorLogicScaffold::CREATED) {
    showStatusMessage("Wrote src/ with an example — Build ▸ Build Game Logic "
                      "compiles it");
  } else if (made == EditorLogicScaffold::ALREADY_THERE) {
    showStatusMessage("This project already has game logic in src/");
  } else {
    showStatusMessage("Could not write the project's src/ folder");
  }
}

void SimplishEditor::buildGameLogic() {
  if (!state_.project.loaded) {
    return;
  }
  const std::filesystem::path& root = state_.project.root;
  // Building a project with no logic starts it some: there is nothing
  // else a build could mean.
  (void)scaffoldProjectLogic(root);
  refreshLogicState();
  reloadUi();
  const EditorToolchain tools = editorToolchain();
  std::vector<EditorBuildCommand> commands = logicBuildCommands(root, tools);
  // The new library runs in a process of its own before this one loads it.
  if (const auto check = logicCheckCommand(root, tools);
      check && bakeLogicCheck()) {
    commands.push_back(*check);
  }
  if (startBuild(EditorBuildKind::LOGIC, std::move(commands))) {
    showStatusMessage("Building game logic…");
  }
}

bool SimplishEditor::bakeLogicCheck() {
  const std::filesystem::path& root = state_.project.root;
  const std::filesystem::path check = projectLogicCheckPath(root);
  std::error_code ec;
  std::filesystem::remove_all(check, ec);
  // Every saved level, for the tests; then the open one as it stands,
  // unsaved edits and all, over its saved self — what the checks run.
  std::vector<std::string> levels = bakeDeployLevels(check);
  if (std::ranges::find(levels, state_.level_id) == levels.end()) {
    levels.push_back(state_.level_id);
  }
  return writeProjectTextFile(
             check / EDITOR_DEPLOY_LEVELS_DIR /
                 (state_.level_id + std::string(EDITOR_SETUP_FILE_SUFFIX)),
             serializeGameSetup(openLevelSetup())) &&
         copyDataTables(root, check) &&
         writeProjectTextFile(
             check / EDITOR_DEPLOY_MANIFEST,
             serializeDeployManifest({"check", levels, state_.level_id, true}));
}

game::GameSetup SimplishEditor::openLevelSetup() {
  game::GameSetup setup = makeEditorPlaytestSetup(
      state_.document, state_.assets, playtestFallback());
  addEditorStandIns(setup, state_.document, sim::MAX_PLAYERS - 1);
  setup.actor_capacity = game::GAME_LOGIC_ACTOR_CAPACITY;
  return setup;
}

void SimplishEditor::deployGame() {
  if (!state_.project.loaded) {
    return;
  }
  if (state_.build.status == EditorBuildStatus::RUNNING) {
    showStatusMessage("A build is already running");
    return;
  }
  if (!bakeDeployContent()) {
    return;
  }
  if (startBuild(EditorBuildKind::DEPLOY,
                 deployBuildCommands(state_.project.root, editorToolchain()))) {
    showStatusMessage(deployStartMessage(state_));
  }
}

bool SimplishEditor::startBuild(EditorBuildKind kind,
                                std::vector<EditorBuildCommand> commands) {
  const std::filesystem::path log =
      projectBuildLogPath(state_.project.root, kind);
  if (!build_job_.start(std::move(commands), log)) {
    showStatusMessage("A build is already running");
    return false;
  }
  building_root_ = state_.project.root;
  EditorBuildState& build = state_.build;
  build.kind = kind;
  build.status = EditorBuildStatus::RUNNING;
  build.builds += 1;
  build.log = log;
  readBuildLog(build, {});
  LOG_INFO("editor", "Build started; its output goes to " + log.string());
  return true;
}

void SimplishEditor::tickBuild() {
  followBuildProject();
  if (state_.build.status != EditorBuildStatus::RUNNING) {
    return;
  }
  const EditorBuildStatus now = build_job_.status();
  if (now == EditorBuildStatus::RUNNING) {
    return;
  }
  if (building_root_ != build_root_) {
    // Its project was closed while it ran: nothing to load, or copy, into.
    state_.build.status = now;
    return;
  }
  finishBuild(now);
}

void SimplishEditor::followBuildProject() {
  const std::filesystem::path root =
      state_.project.loaded ? state_.project.root : std::filesystem::path{};
  if (root == build_root_) {
    return;
  }
  build_root_ = root;
  forgetGameLogic();
  refreshLogicState();
  std::error_code ec;
  if (!root.empty() &&
      std::filesystem::exists(projectLogicLibraryPath(root), ec)) {
    loadGameLogic();
  }
}

void SimplishEditor::forgetGameLogic() {
  logic_library_.reset();
  EditorBuildState& build = state_.build;
  build.logic_loaded = false;
  build.logic_error.clear();
  build.logic_library.clear();
  build.deployed.clear();
}

void SimplishEditor::finishBuild(EditorBuildStatus status) {
  EditorBuildState& build = state_.build;
  build.status = status;
  readBuildLog(build, readLogLines(build.log));
  if (build.kind == EditorBuildKind::LOGIC) {
    finishLogicBuild();
    return;
  }
  if (status == EditorBuildStatus::SUCCEEDED && !finishDeploy()) {
    build.status = EditorBuildStatus::FAILED;
  }
  if (build.status != EditorBuildStatus::SUCCEEDED) {
    LOG_WARN("build", "Deploy failed — see " + build.log.string());
  }
  showStatusMessage(build.status == EditorBuildStatus::SUCCEEDED
                        ? "Deployed to " + build.deployed.string()
                        : "Deploy failed — see " + build.log.string());
}

void SimplishEditor::finishLogicBuild() {
  if (state_.build.status == EditorBuildStatus::SUCCEEDED &&
      state_.project.loaded) {
    loadGameLogic();
  }
  state_.build.tests = parseLogicTestResults(
      readProjectTextFile(projectLogicCheckPath(building_root_) /
                          LOGIC_TEST_RESULTS_FILE)
          .value_or(""));
  refreshLogicState();
  const std::string message = logicBuildMessage(state_.build);
  if (state_.build.status == EditorBuildStatus::FAILED) {
    LOG_WARN("build", message);
  }
  showStatusMessage(message);
}

void SimplishEditor::loadGameLogic() {
  const std::filesystem::path& root = state_.project.root;
  const EditorLogicLibraryLoad load =
      loadEditorLogicLibrary(projectLogicLibraryPath(root),
                             projectLogicLoadPath(root, ++logic_loads_));
  EditorBuildState& build = state_.build;
  build.logic_error = load.error;
  if (load.library == nullptr) {
    LOG_WARN("editor", "Game logic would not load: " + load.error);
    return;
  }
  // A running playtest keeps the library it started with; this one waits
  // for the next.
  logic_library_ = load.library;
  build.logic_loaded = true;
  build.logic_library = logic_library_->path();
  LOG_INFO("editor", "Loaded game logic from " + build.logic_library.string());
}

void SimplishEditor::refreshLogicState() {
  const bool open = state_.project.loaded;
  state_.build.has_logic = open && projectHasLogic(state_.project.root);
  state_.build.logic_stale =
      state_.build.has_logic && projectLogicStale(state_.project.root);
}

bool SimplishEditor::bakeDeployContent() {
  const std::filesystem::path& root = state_.project.root;
  const std::filesystem::path content = deployContentPath(root);
  std::error_code ec;
  std::filesystem::remove_all(content, ec);
  const EditorDeployManifest manifest = deployManifest(
      state_.project.metadata.name, bakeDeployLevels(content), root);
  if (manifest.levels.empty() || !copyDataTables(root, content) ||
      !writeProjectTextFile(content / EDITOR_DEPLOY_MANIFEST,
                            serializeDeployManifest(manifest))) {
    showStatusMessage("Nothing to deploy: save a level first");
    return false;
  }
  return true;
}

std::vector<std::string>
SimplishEditor::bakeDeployLevels(const std::filesystem::path& content) {
  std::vector<std::string> baked;
  for (const EditorLevelEntry& level : state_.levels) {
    const std::optional<game::GameSetup> setup = bakeLevelSetup(level.id);
    const std::filesystem::path file =
        content / EDITOR_DEPLOY_LEVELS_DIR /
        (level.id + std::string(EDITOR_SETUP_FILE_SUFFIX));
    if (setup && writeProjectTextFile(file, serializeGameSetup(*setup))) {
      baked.push_back(level.id);
    }
  }
  return baked;
}

std::optional<game::GameSetup>
SimplishEditor::bakeLevelSetup(const std::string& id) {
  const std::optional<std::string> text =
      readProjectTextFile(editorLevelPath(state_.project.root, id));
  const std::optional<EditorLevelLoad> load =
      text ? parseEditorLevel(*text, state_.assets) : std::nullopt;
  if (!load) {
    LOG_WARN("editor", "Deploy skipped level " + id + ": it will not read");
    return std::nullopt;
  }
  // A prop's collision box is measured from its mesh, which only a load
  // measures.
  for (const EditorPlacement& placement : load->document.placements) {
    (void)ensureAssetMesh(placement.asset);
  }
  game::GameSetup setup =
      makeEditorPlaytestSetup(load->document, state_.assets, {0.5f, 0.5f});
  // Every seat filled: the deployed game plays with as many as it is asked.
  addEditorStandIns(setup, load->document, sim::MAX_PLAYERS - 1);
  return setup;
}

bool SimplishEditor::finishDeploy() {
  const std::filesystem::path& root = state_.project.root;
  const std::filesystem::path built = deployBuiltExecutablePath(root);
  const std::filesystem::path to =
      projectDeployPath(root) / executableFileName(DEPLOYED_GAME_STEM);
  std::error_code ec;
  std::filesystem::copy_file(
      built, to, std::filesystem::copy_options::overwrite_existing, ec);
  if (ec) {
    LOG_WARN("editor", "Deploy built no game at " + built.string());
    return false;
  }
  state_.build.deployed = projectDeployPath(root);
  return true;
}

EditorPlaytestRun SimplishEditor::playtestRun() {
  refreshLogicState();
  return {state_.level_id, logic_library_};
}

std::string SimplishEditor::playtestLogicNote() const {
  const EditorBuildState& build = state_.build;
  if (!build.has_logic) {
    return {};
  }
  if (!build.logic_loaded) {
    return " — game logic not built (Build ▸ Build Game Logic)";
  }
  return build.logic_stale ? " — game logic is older than src/: rebuild it"
                           : std::string{};
}

}  // namespace eng::editor
