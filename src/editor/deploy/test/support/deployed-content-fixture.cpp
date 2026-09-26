#include "support/deployed-content-fixture.h"

#include <editor/build/editor-deploy-manifest.h>
#include <editor/build/editor-setup-json.h>
#include <editor/project/project-text-file.h>
#include <game/world/game-setup.h>
#include <system_error>

namespace eng::editor::test {

namespace {

  /// A folder of its own for the fixture at @p self.
  std::filesystem::path folderFor(const void* self) {
    return std::filesystem::temp_directory_path() /
           ("simplish-deployed-" +
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            std::to_string(reinterpret_cast<uintptr_t>(self)));
  }

  /// The arena's setup, baked as a deploy bakes it: every seat filled.
  game::GameSetup arena() {
    game::GameSetup setup;
    setup.player_count = 4;
    setup.spawns = {{{1.5F, 1.5F, 0.0F},
                     {2.5F, 1.5F, 0.0F},
                     {1.5F, 2.5F, 0.0F},
                     {2.5F, 2.5F, 0.0F}}};
    game::ActorSpawn actor;
    actor.at = {9.5F, 9.5F, 0.0F};
    actor.behavior = "idle";
    setup.actors.push_back(actor);
    return setup;
  }

}  // namespace

DeployedContentFixture::DeployedContentFixture(const std::string& extra)
  : path_(folderFor(this)) {
  (void)writeProjectTextFile(path_ / "levels" / "arena.setup.json",
                             serializeGameSetup(arena()));
  (void)writeProjectTextFile(
      path_ / EDITOR_DEPLOY_MANIFEST,
      serializeDeployManifest({"Arena", {"arena"}, "arena", true}));
  if (!extra.empty()) {
    (void)writeProjectTextFile(path_ / "content" / "extra.txt", extra);
  }
}

DeployedContentFixture::~DeployedContentFixture() {
  std::error_code ec;
  std::filesystem::remove_all(path_, ec);
}

DeployedGameOptions DeployedContentFixture::options(uint64_t ticks) const {
  DeployedGameOptions options;
  options.content = path_;
  options.max_ticks = ticks;
  return options;
}

}  // namespace eng::editor::test
