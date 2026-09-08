#include <cstdlib>
#include <editor/project/project-paths.h>
#include <editor/shell/simplish-editor.h>
#include <engine/client/desktop-user-data-path.h>
#include <engine/client/game-client-config.h>
#include <engine/core/logger.h>
#include <filesystem>
#include <string>
#include <string_view>

namespace {

/// Organisation and application names the user's data directory is keyed
/// on. Changing either strands whatever is already stored under the old
/// pair, so they are constants rather than anything configurable.
constexpr std::string_view APP_ORG = "Simplish";
constexpr std::string_view APP_NAME = "Editor";
/// The recent-projects list, inside that directory.
constexpr std::string_view RECENT_PROJECTS_FILE = "recent-projects.json";
/// Where it used to live, and where it still goes when the platform offers
/// nowhere better.
constexpr std::string_view RECENT_PROJECTS_FALLBACK =
    "editor/recent-projects.json";

/// Resolve the engine data directory: `ENGINE_DATA_DIR` when set, else "data".
std::string resolveDataDir() {
  // NOLINTNEXTLINE(concurrency-mt-unsafe) -- main-thread-only startup read
  if (const char* dir = std::getenv("ENGINE_DATA_DIR")) {
    return dir;
  }
  return "data";
}

/// Where the recent-projects list is read from and written to.
///
/// The user's own application data, not the data directory: the list is
/// something the editor writes on every run, and it belongs to the person
/// running it rather than to the copy of the editor they happen to launch.
/// Keeping it beside the shipped data made it a file that changed under
/// version control every time the editor was opened.
std::filesystem::path resolveRecentProjectsPath() {
  const std::filesystem::path user_data =
      eng::client::desktopUserDataPath(APP_ORG, APP_NAME);
  if (user_data.empty()) {
    LOG_WARN("editor",
             "No user data directory; keeping the recent-projects list "
             "beside the engine data instead.");
    return std::filesystem::path(resolveDataDir()) / RECENT_PROJECTS_FALLBACK;
  }
  return user_data / RECENT_PROJECTS_FILE;
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

  editor.setRecentProjectsPath(resolveRecentProjectsPath());

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
