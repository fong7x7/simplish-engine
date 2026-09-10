#include <editor/project/project-paths.h>
#include <editor/shell/editor-placement-transform.h>
#include <editor/shell/editor-player-start-ops.h>
#include <editor/shell/editor-playtest-session.h>
#include <engine/sim/replay-codec.h>
#include <fstream>
#include <span>
#include <system_error>
#include <utility>

namespace eng::editor {

namespace {

  /// The first start for player 1, in document order, or nothing.
  const EditorPlayerStart*
  firstStartForPlayerOne(const EditorDocument& document) {
    for (const EditorPlayerStart& start : document.player_starts) {
      if (start.player == 1) {
        return &start;
      }
    }
    return nullptr;
  }

  /// The input the next tick runs on: player 1's is the oldest scripted
  /// input while any is queued, and @p live otherwise; the others are idle.
  /// Consumes one tick of the scripted input it uses.
  sim::TickInput nextTickInput(const sim::PlayerInput& live,
                               std::vector<EditorScriptedInput>& scripted) {
    sim::TickInput input;
    if (scripted.empty()) {
      input.players[0] = live;
      return input;
    }
    input.players[0] = scripted.front().input;
    if (--scripted.front().ticks == 0) {
      scripted.erase(scripted.begin());
    }
    return input;
  }

  /// A stand-in for an asset a placement names but the list no longer has,
  /// which `placementWorldBounds` measures as the unit box on its tile.
  const EditorAsset MISSING_ASSET{};

  /// The collision box of every placement that collides, in document order.
  std::vector<physics::CollisionBox>
  obstaclesOf(const EditorDocument& document,
              const std::vector<EditorAsset>& assets) {
    std::vector<physics::CollisionBox> boxes;
    for (const EditorPlacement& placement : document.placements) {
      if (!placement.collides) {
        continue;
      }
      const EditorAsset& asset = placement.asset < assets.size()
                                     ? assets[placement.asset]
                                     : MISSING_ASSET;
      const PlacementBounds bounds = placementWorldBounds(asset, placement);
      boxes.push_back({bounds.min, bounds.max});
    }
    return boxes;
  }

  /// A replay header for a playtest of @p level_id set up as @p setup.
  sim::ReplayHeader replayHeader(const game::GameSetup& setup,
                                 const std::string& level_id) {
    sim::ReplayHeader header;
    header.level_id = level_id;
    header.seed = setup.seed;
    header.player_count = setup.player_count;
    return header;
  }

  Vec3 lerp(Vec3 from, Vec3 to, float alpha) {
    return {from.x + (to.x - from.x) * alpha, from.y + (to.y - from.y) * alpha,
            from.z + (to.z - from.z) * alpha};
  }

}  // namespace

game::GameSetup makeEditorPlaytestSetup(const EditorDocument& document,
                                        const std::vector<EditorAsset>& assets,
                                        WorldPoint fallback) {
  const EditorPlayerStart* start = firstStartForPlayerOne(document);
  const WorldPoint at = start != nullptr ? start->position : fallback;
  game::GameSetup setup;
  setup.seed = EDITOR_PLAYTEST_SEED;
  setup.player_count = 1;
  setup.spawns[0] = {at.x, at.y, at.z};
  setup.obstacles = obstaclesOf(document, assets);
  return setup;
}

std::filesystem::path
editorPlaytestReplayPath(const std::filesystem::path& root,
                         std::string_view level_id) {
  return projectDataPath(root) / "playtests" /
         (std::string(level_id) + ".replay");
}

bool writeEditorPlaytestReplay(const std::filesystem::path& root,
                               std::string_view level_id,
                               const sim::Replay& replay) {
  const std::filesystem::path path = editorPlaytestReplayPath(root, level_id);
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  const std::vector<std::byte> bytes = sim::encodeReplay(replay);
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
  return static_cast<bool>(out);
}

EditorPlaytestSession::EditorPlaytestSession(const game::GameSetup& setup,
                                             const std::string& level_id)
  : world_(std::make_unique<game::GameWorld>(setup)),
    simulation_(*world_, sim::TickHashing::ON),
    recorder_(replayHeader(setup, level_id), sim::DEFAULT_CHECKPOINT_INTERVAL),
    previous_(world_->players().position) {}

FixedStepAdvance
EditorPlaytestSession::advance(uint64_t elapsed_ns,
                               const sim::PlayerInput& live,
                               std::vector<EditorScriptedInput>& scripted) {
  const FixedStepAdvance due = clock_.advance(elapsed_ns);
  dropped_ticks_ += due.dropped_ticks;
  for (uint32_t i = 0; i < due.ticks; ++i) {
    step(live, scripted);
  }
  return due;
}

void EditorPlaytestSession::step(const sim::PlayerInput& live,
                                 std::vector<EditorScriptedInput>& scripted) {
  previous_ = world_->players().position;
  const sim::TickInput input = nextTickInput(live, scripted);
  const sim::TickResult result = simulation_.step(input);
  recorder_.record(input, result);
  last_hash_ = result.hash;
}

void EditorPlaytestSession::publish(EditorPlaytestState& state) const {
  const game::PlayerPool& pool = world_->players();
  state.tick = tick();
  state.dropped_ticks = dropped_ticks_;
  state.hash = last_hash_ ? std::optional{last_hash_->combined} : std::nullopt;
  state.players.clear();
  for (uint32_t i = 0; i < pool.slots.size(); ++i) {
    const Vec3& at = pool.position[i];
    state.players.push_back(
        {static_cast<uint8_t>(pool.input_slot[i] + 1U), {at.x, at.y, at.z}});
  }
}

Vec3 EditorPlaytestSession::renderPosition(size_t index, float alpha) const {
  const Vec3 now = world_->players().position[index];
  return index < previous_.size() ? lerp(previous_[index], now, alpha) : now;
}

const game::PlayerPool& EditorPlaytestSession::players() const {
  return world_->players();
}

}  // namespace eng::editor
