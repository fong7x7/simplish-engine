#include <algorithm>
#include <editor/project/project-paths.h>
#include <editor/shell/editor-actor-placement.h>
#include <editor/shell/editor-character-choices.h>
#include <editor/shell/editor-placement-transform.h>
#include <editor/shell/editor-player-start-ops.h>
#include <editor/shell/editor-playtest-session.h>
#include <editor/shell/editor-waypoint-ops.h>
#include <engine/sim/replay-codec.h>
#include <fstream>
#include <game/content/character-lookup.h>
#include <span>
#include <system_error>
#include <utility>

namespace eng::editor {

namespace {

  /// The first start for @p player, in document order, or nothing.
  const EditorPlayerStart* firstStartFor(const EditorDocument& document,
                                         uint8_t player) {
    for (const EditorPlayerStart& start : document.player_starts) {
      if (start.player == player) {
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

  /// @p placement's asset in @p assets, or the stand-in for one it lost.
  const EditorAsset& assetOf(const EditorPlacement& placement,
                             const std::vector<EditorAsset>& assets) {
    return placement.asset < assets.size() ? assets[placement.asset]
                                           : MISSING_ASSET;
  }

  /// The collision box of every placement that collides and is not an
  /// actor, in document order.
  std::vector<physics::CollisionBox>
  obstaclesOf(const EditorDocument& document,
              const std::vector<EditorAsset>& assets) {
    std::vector<physics::CollisionBox> boxes;
    for (const EditorPlacement& placement : document.placements) {
      if (placement.collides && !isEditorActor(placement)) {
        const PlacementBounds bounds =
            placementWorldBounds(assetOf(placement, assets), placement);
        boxes.push_back({bounds.min, bounds.max});
      }
    }
    return boxes;
  }

  /// The actor every placement with a behavior is, in document order.
  std::vector<game::ActorSpawn>
  actorsOf(const EditorDocument& document,
           const std::vector<EditorAsset>& assets) {
    std::vector<game::ActorSpawn> actors;
    for (const size_t index : editorActorPlacements(document)) {
      const EditorPlacement& placement = document.placements[index];
      actors.push_back(editorActorSpawn(placement, assetOf(placement, assets)));
      // A route with no waypoints left in it is no route: the actor's
      // patrol stands still, as a behavior with no route says it does.
      if (placement.route != 0) {
        actors.back().route = editorRoutePoints(document, placement.route);
      }
    }
    return actors;
  }

  /// The waypoints of @p path still to walk, on the floor at @p z.
  std::vector<WorldPoint> remainingPath(const game::ActorPath& path, float z) {
    std::vector<WorldPoint> points;
    for (uint32_t i = path.next; i < path.count && i < path.points.size();
         ++i) {
      points.push_back({path.points[i].x, path.points[i].y, z});
    }
    return points;
  }

  /// The number of the player @p handle names in @p players, 1 to 4, or 0
  /// for none.
  uint8_t playerNumber(const game::PlayerPool& players,
                       sim::EntityHandle handle) {
    const std::optional<uint32_t> index = players.slots.denseIndex(handle);
    return index ? static_cast<uint8_t>(players.input_slot[*index] + 1U) : 0;
  }

  /// The character each player's first start names, by input slot, as the
  /// bare ids a setup holds.
  std::array<std::string, sim::MAX_PLAYERS>
  startCharacters(const EditorDocument& document) {
    std::array<std::string, sim::MAX_PLAYERS> characters{};
    for (size_t slot = 0; slot < characters.size(); ++slot) {
      const EditorPlayerStart* start =
          firstStartFor(document, static_cast<uint8_t>(slot + 1U));
      if (start != nullptr) {
        characters[slot] = editorCharacterIdOf(start->character);
      }
    }
    return characters;
  }

  /// A replay header for a playtest of @p level_id set up as @p setup.
  sim::ReplayHeader replayHeader(const game::GameSetup& setup,
                                 const std::string& level_id) {
    sim::ReplayHeader header;
    header.level_id = level_id;
    header.seed = setup.seed;
    header.player_count = setup.player_count;
    header.characters = setup.characters;
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
  const EditorPlayerStart* start = firstStartFor(document, 1);
  const WorldPoint at = start != nullptr ? start->position : fallback;
  game::GameSetup setup;
  setup.seed = EDITOR_PLAYTEST_SEED;
  setup.player_count = 1;
  setup.spawns[0] = {at.x, at.y, at.z};
  setup.characters = startCharacters(document);
  setup.obstacles = obstaclesOf(document, assets);
  setup.actors = actorsOf(document, assets);
  return setup;
}

std::string editorPlaytestDefaultCharacter(
    const EditorDocument& document,
    const std::vector<game::CharacterDefinition>& characters) {
  const EditorPlayerStart* start = firstStartFor(document, 1);
  if (start != nullptr) {
    if (const std::optional<size_t> named =
            findEditorCharacter(characters, start->character)) {
      return characters[*named].id;
    }
  }
  return characters.empty() ? std::string{} : characters.front().id;
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
                                             const game::GameContent& content,
                                             const std::string& level_id)
  : world_(std::make_unique<game::GameWorld>(setup, content)),
    simulation_(*world_, sim::TickHashing::ON),
    recorder_(replayHeader(setup, level_id), sim::DEFAULT_CHECKPOINT_INTERVAL),
    content_(content), characters_(setup.characters),
    previous_(world_->players().position),
    previous_actors_(world_->actors().position) {}

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
  previous_actors_ = world_->actors().position;
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
    state.players.push_back({static_cast<uint8_t>(pool.input_slot[i] + 1U),
                             {at.x, at.y, at.z},
                             character(i).id,
                             pool.health[i]});
  }
  publishActors(state);
}

void EditorPlaytestSession::publishActors(EditorPlaytestState& state) const {
  state.actors.clear();
  for (size_t actor = 0; actor < actorCount(); ++actor) {
    if (const std::optional<uint32_t> index = actorIndex(actor)) {
      state.actors.push_back(actorReport(actor, *index));
    }
  }
}

EditorPlaytestActor EditorPlaytestSession::actorReport(size_t actor,
                                                       uint32_t index) const {
  const game::ActorPool& pool = world_->actors();
  const Vec3& at = pool.position[index];
  const game::ActorPath& path = pool.path[index];
  const bool on_actor = pool.target_kind[index] == game::ActorTargetKind::ACTOR;
  return {.id = actor < actor_ids_.size() ? actor_ids_[actor] : std::string{},
          .position = {at.x, at.y, at.z},
          .facing = pool.facing[index],
          .behavior = world_->brains()[pool.brain[index]].behavior.id,
          .state = actorState(index).id,
          .faction = pool.faction[index],
          .target = on_actor
                        ? uint8_t{0}
                        : playerNumber(world_->players(), pool.target[index]),
          .target_actor = on_actor ? actorIdOf(pool.target[index]) : "",
          .sees_target = pool.sees_target[index] != 0,
          .path_waypoints = path.count - std::min(path.next, path.count)};
}

std::string EditorPlaytestSession::actorIdOf(sim::EntityHandle handle) const {
  const auto handles = world_->actorHandles();
  for (size_t k = 0; k < handles.size() && k < actor_ids_.size(); ++k) {
    if (handles[k] == handle) {
      return actor_ids_[k];
    }
  }
  return {};
}

Vec3 EditorPlaytestSession::renderPosition(size_t index, float alpha) const {
  const Vec3 now = world_->players().position[index];
  return index < previous_.size() ? lerp(previous_[index], now, alpha) : now;
}

const game::PlayerPool& EditorPlaytestSession::players() const {
  return world_->players();
}

const game::CharacterDefinition&
EditorPlaytestSession::character(size_t index) const {
  const uint8_t slot = world_->players().input_slot[index];
  return game::resolveCharacter(content_,
                                characters_[slot % characters_.size()]);
}

void EditorPlaytestSession::setActorIds(std::vector<std::string> ids) {
  actor_ids_ = std::move(ids);
}

size_t EditorPlaytestSession::actorCount() const {
  return world_->actorHandles().size();
}

std::optional<uint32_t> EditorPlaytestSession::actorIndex(size_t actor) const {
  if (actor >= actorCount()) {
    return std::nullopt;
  }
  return world_->actors().slots.denseIndex(world_->actorHandles()[actor]);
}

const game::ActorPool& EditorPlaytestSession::actors() const {
  return world_->actors();
}

Vec3 EditorPlaytestSession::actorRenderPosition(uint32_t index,
                                                float alpha) const {
  const Vec3 now = world_->actors().position[index];
  return index < previous_actors_.size()
             ? lerp(previous_actors_[index], now, alpha)
             : now;
}

EditorCharacterGait EditorPlaytestSession::actorGait(uint32_t index) const {
  const Vec3 now = world_->actors().position[index];
  if (index >= previous_actors_.size()) {
    return EditorCharacterGait::STILL;
  }
  const Vec3 before = previous_actors_[index];
  return now.x != before.x || now.y != before.y ? EditorCharacterGait::MOVING
                                                : EditorCharacterGait::STILL;
}

const game::BehaviorState&
EditorPlaytestSession::actorState(uint32_t index) const {
  const game::ActorPool& pool = world_->actors();
  return world_->brains()[pool.brain[index]].behavior.states[pool.state[index]];
}

EditorActorOverlay EditorPlaytestSession::actorOverlay(uint32_t index,
                                                       float alpha) const {
  const game::ActorPool& pool = world_->actors();
  const game::BehaviorSenses& senses =
      world_->brains()[pool.brain[index]].behavior.senses;
  const Vec3 at = actorRenderPosition(index, alpha);
  return {.at = {at.x, at.y, at.z},
          .height = pool.height[index],
          .facing = pool.facing[index],
          .view_degrees = senses.view_degrees,
          .sight_range = senses.sight_range,
          .path = remainingPath(pool.path[index], at.z),
          .has_target = pool.remembers_target[index] != 0,
          .target = {pool.last_seen[index].x, pool.last_seen[index].y, at.z},
          .sees_target = pool.sees_target[index] != 0,
          .label = actorState(index).id,
          .faction = pool.faction[index]};
}

EditorCharacterGait EditorPlaytestSession::gait(size_t index) const {
  const Vec3 now = world_->players().position[index];
  if (index >= previous_.size()) {
    return EditorCharacterGait::STILL;
  }
  const Vec3 before = previous_[index];
  return now.x != before.x || now.y != before.y ? EditorCharacterGait::MOVING
                                                : EditorCharacterGait::STILL;
}

}  // namespace eng::editor
