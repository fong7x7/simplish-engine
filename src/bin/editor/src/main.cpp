#include <cstdlib>
#include <editor/project/project-paths.h>
#include <editor/shell/simplish-editor.h>
#include <engine/client/game-client-config.h>
#include <engine/core/logger.h>
#include <filesystem>
#include <string>
#include <string_view>

namespace {

/// Where the recent-projects list lives, relative to the data directory.
constexpr std::string_view RECENT_PROJECTS_FILE = "editor/recent-projects.json";

/// Resolve the engine data directory: `ENGINE_DATA_DIR` when set, else "data".
std::string resolveDataDir() {
  // NOLINTNEXTLINE(concurrency-mt-unsafe) -- main-thread-only startup read
  if (const char* dir = std::getenv("ENGINE_DATA_DIR")) {
    return dir;
  }
  return "data";
}

void configureClient(eng::client::GameClientConfig& config) {
  config.window_title = "Simplish Editor";
  config.data_dir = resolveDataDir();
}

}  // namespace

/// Usage: simplish-editor [project-directory]
///
/// With no argument the editor opens with no project loaded. That is a
/// supported state, not an error — the toolbar and viewport are live either
/// way, and a project can be opened later.
int main(int argc, char** argv) {  // NOLINT(bugprone-exception-escape)
  eng::editor::SimplishEditor editor;

  eng::client::GameClientConfig config;
  configureClient(config);

  editor.setRecentProjectsPath(std::filesystem::path(config.data_dir) /
                               RECENT_PROJECTS_FILE);

  auto error = editor.init(config);
  if (error.has_value()) {
    LOG_ERROR("editor", *error);
    return 1;
  }

  if (argc > 1) {
    // A failed open is already logged with its reason; the editor stays up
    // with no project rather than exiting on a bad path.
    (void)editor.openProjectAt(std::filesystem::path(argv[1]));
  }

  editor.run();
  editor.shutdown();
  return 0;
}
