#include <algorithm>
#include <editor/build/editor-setup-json.h>
#include <game/content/behavior-names.h>
#include <game/content/footstep-names.h>
#include <nlohmann/json.hpp>

namespace eng::editor {

namespace {

  using nlohmann::json;

  json vec3(const Vec3& v) { return json::array({v.x, v.y, v.z}); }
  json vec2(const Vec2& v) { return json::array({v.x, v.y}); }

  /// The number at @p key of @p object, or @p fallback.
  template <typename T>
  T number(const json& object, std::string_view key, T fallback) {
    const auto found = object.find(key);
    return found != object.end() && found->is_number() ? found->get<T>()
                                                       : fallback;
  }

  /// The string at @p key of @p object, or empty.
  std::string text(const json& object, std::string_view key) {
    const auto found = object.find(key);
    return found != object.end() && found->is_string()
               ? found->get<std::string>()
               : std::string{};
  }

  /// @p value as a point of @p N floats, missing axes zero.
  template <size_t N> std::array<float, N> floats(const json& value) {
    std::array<float, N> out{};
    for (size_t axis = 0; value.is_array() && axis < N; ++axis) {
      if (axis < value.size() && value[axis].is_number()) {
        out[axis] = value[axis].get<float>();
      }
    }
    return out;
  }

  Vec3 readVec3(const json& value) {
    const auto f = floats<3>(value);
    return {f[0], f[1], f[2]};
  }

  Vec2 readVec2(const json& value) {
    const auto f = floats<2>(value);
    return {f[0], f[1]};
  }

  /// The member @p key of @p object, or null.
  const json& member(const json& object, std::string_view key) {
    static const json none;
    const auto found = object.find(key);
    return found != object.end() ? *found : none;
  }

  /// @p actor's route, as points.
  json writeRoute(const game::ActorSpawn& actor) {
    json route = json::array();
    for (const Vec2& point : actor.route) {
      route.push_back(vec2(point));
    }
    return route;
  }

  json writeActor(const game::ActorSpawn& actor) {
    return {{"id", actor.id},
            {"at", vec3(actor.at)},
            {"yaw_degrees", actor.yaw_degrees},
            {"behavior", actor.behavior},
            {"faction", game::factionName(actor.faction)},
            {"radius", actor.radius},
            {"height", actor.height},
            {"route", writeRoute(actor)},
            {"health", actor.health},
            {"death_blast_radius", actor.death_blast_radius},
            {"death_blast_damage", actor.death_blast_damage},
            {"footsteps", game::stepSetWord(actor.footsteps)},
            {"model", actor.model}};
  }

  /// @p entry's health, death blast and feet, into @p actor.
  void readActorHealth(const json& entry, game::ActorSpawn& actor) {
    actor.health = number(entry, "health", game::ACTOR_DEFAULT_HEALTH);
    actor.death_blast_radius = number(entry, "death_blast_radius", 0.0F);
    actor.death_blast_damage = number(entry, "death_blast_damage", uint16_t{0});
    actor.footsteps = game::stepSetNamed(text(entry, "footsteps"))
                          .value_or(game::StepSet::DEFAULT);
  }

  game::ActorSpawn readActor(const json& entry) {
    game::ActorSpawn actor;
    actor.at = readVec3(member(entry, "at"));
    actor.yaw_degrees = number(entry, "yaw_degrees", 0.0F);
    actor.behavior = text(entry, "behavior");
    actor.faction = game::parseFaction(text(entry, "faction"))
                        .value_or(game::Faction::HOSTILE);
    actor.radius = number(entry, "radius", game::ACTOR_DEFAULT_RADIUS_TILES);
    actor.height = number(entry, "height", game::ACTOR_DEFAULT_HEIGHT_TILES);
    for (const json& point : member(entry, "route")) {
      actor.route.push_back(readVec2(point));
    }
    readActorHealth(entry, actor);
    actor.id = text(entry, "id");
    actor.model = text(entry, "model");
    return actor;
  }

  json writePlayers(const game::GameSetup& setup) {
    json spawns = json::array();
    json characters = json::array();
    for (size_t slot = 0; slot < sim::MAX_PLAYERS; ++slot) {
      spawns.push_back(vec3(setup.spawns[slot]));
      characters.push_back(setup.characters[slot]);
    }
    return {{"spawns", spawns}, {"characters", characters}};
  }

  void readPlayers(const json& root, game::GameSetup& setup) {
    const json& spawns = member(root, "spawns");
    const json& characters = member(root, "characters");
    for (size_t slot = 0; slot < sim::MAX_PLAYERS; ++slot) {
      if (spawns.is_array() && slot < spawns.size()) {
        setup.spawns[slot] = readVec3(spawns[slot]);
      }
      if (characters.is_array() && slot < characters.size() &&
          characters[slot].is_string()) {
        setup.characters[slot] = characters[slot].get<std::string>();
      }
    }
  }

  json writeObstacles(const game::GameSetup& setup) {
    json boxes = json::array();
    for (const physics::CollisionBox& box : setup.obstacles) {
      boxes.push_back({{"min", vec3(box.min)}, {"max", vec3(box.max)}});
    }
    return boxes;
  }

  /// The obstacles and actors of @p root into @p setup.
  void readBodies(const json& root, game::GameSetup& setup) {
    for (const json& box : member(root, "obstacles")) {
      setup.obstacles.push_back(
          {readVec3(member(box, "min")), readVec3(member(box, "max"))});
    }
    for (const json& actor : member(root, "actors")) {
      setup.actors.push_back(readActor(actor));
    }
  }

  json writeActors(const game::GameSetup& setup) {
    json actors = json::array();
    for (const game::ActorSpawn& actor : setup.actors) {
      actors.push_back(writeActor(actor));
    }
    return actors;
  }

}  // namespace

std::string serializeGameSetup(const game::GameSetup& setup) {
  const json root = {{"schema", EDITOR_SETUP_SCHEMA},
                     {"seed", setup.seed},
                     {"player_count", setup.player_count},
                     {"actor_capacity", setup.actor_capacity},
                     {"players", writePlayers(setup)},
                     {"obstacles", writeObstacles(setup)},
                     {"actors", writeActors(setup)}};
  return root.dump(2) + "\n";
}

std::optional<game::GameSetup> parseGameSetup(std::string_view text_in) {
  const json root = json::parse(text_in, nullptr, false);
  if (root.is_discarded() || !root.is_object() ||
      text(root, "schema") != EDITOR_SETUP_SCHEMA) {
    return std::nullopt;
  }
  game::GameSetup setup;
  setup.seed = number(root, "seed", uint64_t{0});
  setup.player_count = number(root, "player_count", uint8_t{1});
  setup.actor_capacity = number(root, "actor_capacity", uint32_t{0});
  readPlayers(member(root, "players"), setup);
  readBodies(root, setup);
  return setup;
}

}  // namespace eng::editor
