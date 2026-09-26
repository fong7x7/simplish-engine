#include "deployed-level.h"

#include <editor/build/editor-deploy-manifest.h>
#include <editor/deploy/deployed-content.h>
#include <editor/project/project-text-file.h>
#include <string>

namespace eng::editor {

std::optional<game::GameSetup> manifestSetup(const DeployedGameOptions& options,
                                             DeployedGameRun& run) {
  const std::optional<std::string> text =
      readProjectTextFile(options.content / EDITOR_DEPLOY_MANIFEST);
  const auto manifest = text ? parseDeployManifest(*text) : std::nullopt;
  run.level =
      options.level.empty() && manifest ? manifest->start_level : options.level;
  auto setup =
      manifest ? readDeployedSetup(options.content, run.level) : std::nullopt;
  if (!setup) {
    run.error = manifest ? "No level " + run.level + " in this game"
                         : "No deployed game at " + options.content.string();
  }
  return setup;
}

}  // namespace eng::editor
