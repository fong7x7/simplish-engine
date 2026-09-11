#include <array>
#include <cstddef>
#include <editor/shell/editor-behavior-choices.h>
#include <editor/shell/editor-character-choices.h>
#include <editor/shell/editor-entity-id.h>
#include <editor/shell/editor-level-json.h>
#include <editor/shell/editor-light-ops.h>
#include <editor/shell/editor-player-start-ops.h>
#include <editor/shell/editor-property-ops.h>
#include <editor/shell/editor-waypoint-ops.h>
#include <game/content/behavior-names.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// A position, rotation, direction or colour: three numbers, one shape.
  using Triple = std::array<float, 3>;

  /// What a missing triple reads as, for a field with no better default.
  constexpr Triple ZERO_TRIPLE{0.0f, 0.0f, 0.0f};

  /// Parse without throwing — exceptions are disabled here (ADR-001), so
  /// failure comes back as `is_discarded()`.
  std::optional<json> tryParse(std::string_view text) {
    json parsed = json::parse(std::string(text), nullptr, false);
    if (parsed.is_discarded()) {
      return std::nullopt;
    }
    return parsed;
  }

  json tripleJson(float a, float b, float c) {
    return json::array({a, b, c});
  }

  /// The three numbers under @p key, or @p fallback for anything the file
  /// did not write as three numbers. Read component by component so a file
  /// with one bad entry keeps the other two.
  Triple readTriple(const json& entry, const char* key, Triple fallback) {
    const auto found = entry.find(key);
    if (found == entry.end() || !found->is_array() || found->size() != 3) {
      return fallback;
    }
    for (size_t axis = 0; axis < fallback.size(); ++axis) {
      if ((*found)[axis].is_number()) {
        fallback[axis] = (*found)[axis].get<float>();
      }
    }
    return fallback;
  }

  /// One number under @p key, or @p fallback when it is absent or is not a
  /// number. Type-checked rather than trusted: a level file is meant to be
  /// editable by hand ([project-format.md §1]).
  float readNumber(const json& entry, const char* key, float fallback) {
    const auto found = entry.find(key);
    return found != entry.end() && found->is_number() ? found->get<float>()
                                                      : fallback;
  }

  /// One boolean under @p key, or nothing when it is absent or is not one.
  std::optional<bool> readBool(const json& entry, const char* key) {
    const auto found = entry.find(key);
    if (found == entry.end() || !found->is_boolean()) {
      return std::nullopt;
    }
    return found->get<bool>();
  }

  /// One string under @p key, or empty when it is absent or is not one.
  std::string readString(const json& entry, const char* key) {
    const auto found = entry.find(key);
    return found != entry.end() && found->is_string()
               ? found->get<std::string>()
               : std::string{};
  }

  /// How a prop names its asset: the qualified reference, empty for an
  /// asset the list no longer holds, which is what a reader drops on.
  std::string assetRefAt(const std::vector<EditorAsset>& assets, size_t index) {
    return index < assets.size() ? editorAssetRef(assets[index])
                                 : std::string{};
  }

  /// The keys a prop carries only when they say something, into @p out.
  void addPropExtras(json& out, const EditorPlacement& placement) {
    // Only when it is not the size the prop was dropped at, so a level saved
    // before scale existed saves back byte for byte rather than every prop
    // in it gaining a line that says nothing.
    if (placement.scale != 1.0f) {
      out["scale"] = placement.scale;
    }
    out["collides"] = placement.collides;
    // Only when it names one: empty means the model's first clip, and a
    // static prop, which is most of them, has no business carrying the key.
    if (!placement.animation.empty()) {
      out["animation"] = placement.animation;
    }
    // Only for an actor: a prop with no behavior is scenery, and a faction
    // on scenery would be a line saying nothing.
    if (!placement.behavior.empty()) {
      out["behavior"] = placement.behavior;
      out["faction"] = game::factionName(placement.faction);
    }
    if (!placement.behavior.empty() && placement.route != 0) {
      out["route"] = placement.route;
    }
  }

  json propJson(const EditorPlacement& placement,
                const std::vector<EditorAsset>& assets) {
    json out;
    out["id"] = placement.id;
    out["asset"] = assetRefAt(assets, placement.asset);
    out["at"] = tripleJson(placement.position.x, placement.position.y,
                           placement.position.z);
    out["rotation"] = tripleJson(placement.rotation.x, placement.rotation.y,
                                 placement.rotation.z);
    addPropExtras(out, placement);
    return out;
  }

  json lightJson(const EditorLight& light) {
    json out;
    out["id"] = light.id;
    out["kind"] = editorLightKindId(light.kind);
    out["at"] =
        tripleJson(light.position.x, light.position.y, light.position.z);
    out["direction"] =
        tripleJson(light.direction.x, light.direction.y, light.direction.z);
    out["color"] = tripleJson(light.color.x, light.color.y, light.color.z);
    out["intensity"] = light.intensity;
    out["range"] = light.range;
    return out;
  }

  /// A player start, as the format's entity shape: a definition naming what
  /// it is and a property block holding what it carries
  /// ([project-format.md §4]).
  json playerStartJson(const EditorPlayerStart& start) {
    json out;
    out["id"] = start.id;
    out["definition"] = EDITOR_PLAYER_START_DEFINITION;
    out["at"] =
        tripleJson(start.position.x, start.position.y, start.position.z);
    out["properties"] = {{"player", start.player}};
    // Only when it names one: a start with none draws the stand-in, and
    // most levels' starts are that.
    if (!start.character.empty()) {
      out["properties"]["character"] = start.character;
    }
    return out;
  }

  /// A waypoint, as the same entity shape: which route it belongs to and
  /// its place in it.
  json waypointJson(const EditorWaypoint& waypoint) {
    json out;
    out["id"] = waypoint.id;
    out["definition"] = EDITOR_WAYPOINT_DEFINITION;
    out["at"] = tripleJson(waypoint.position.x, waypoint.position.y,
                           waypoint.position.z);
    out["properties"] = {{"route", waypoint.route}, {"order", waypoint.order}};
    return out;
  }

  json entitiesJson(const EditorDocument& document) {
    json entities = json::array();
    for (const EditorPlayerStart& start : document.player_starts) {
      entities.push_back(playerStartJson(start));
    }
    for (const EditorWaypoint& waypoint : document.waypoints) {
      entities.push_back(waypointJson(waypoint));
    }
    return entities;
  }

  json propsJson(const EditorDocument& document,
                 const std::vector<EditorAsset>& assets) {
    json props = json::array();
    for (const EditorPlacement& placement : document.placements) {
      props.push_back(propJson(placement, assets));
    }
    return props;
  }

  json lightsJson(const EditorDocument& document) {
    json lights = json::array();
    for (const EditorLight& light : document.lights) {
      lights.push_back(lightJson(light));
    }
    return lights;
  }

  /// Every asset by every string a file may name it with: the qualified
  /// reference the editor writes, and the bare id, which is what somebody
  /// editing the file by hand is likely to reach for.
  std::unordered_map<std::string, size_t>
  assetsByReference(const std::vector<EditorAsset>& assets) {
    std::unordered_map<std::string, size_t> by_ref;
    by_ref.reserve(assets.size() * 2);
    for (size_t index = 0; index < assets.size(); ++index) {
      // An asset with no id of its own is unnameable, and mapping its empty
      // string would make every prop that names nothing resolve to it.
      if (assets[index].id.empty()) {
        continue;
      }
      by_ref.emplace(editorAssetRef(assets[index]), index);
      by_ref.emplace(assets[index].id, index);
    }
    return by_ref;
  }

  /// One prop, bound to the asset at @p index.
  ///
  /// An id is minted when the file left one out, so that no route into the
  /// document can produce a placement nothing is able to name — the rule
  /// `addPlacement` follows on the agent side.
  /// @p behavior as the qualified reference the editor holds and writes: a
  /// hand-written bare id, `guard`, reads as `behavior:guard`. Anything
  /// already qualified is kept as written, as a character is.
  std::string behaviorRef(const std::string& behavior) {
    return behavior.empty() || behavior.contains(':')
               ? behavior
               : editorBehaviorRef(behavior);
  }

  /// The behavior, faction and route of a prop, into @p placement. A
  /// faction the format does not know reads as hostile, the side a prop
  /// given a behavior starts on; a route below 1 reads as none.
  void readActor(const json& entry, EditorPlacement& placement) {
    placement.behavior = behaviorRef(readString(entry, "behavior"));
    placement.faction = game::parseFaction(readString(entry, "faction"))
                            .value_or(game::Faction::HOSTILE);
    const float route = readNumber(entry, "route", 0.0f);
    placement.route = route >= 0.5f ? clampEditorRoute(route) : uint8_t{0};
  }

  EditorPlacement readProp(const json& entry, size_t index,
                           const EditorDocument& document,
                           const std::vector<EditorAsset>& assets) {
    const Triple at = readTriple(entry, "at", ZERO_TRIPLE);
    const Triple turn = readTriple(entry, "rotation", ZERO_TRIPLE);
    EditorPlacement placement;
    placement.asset = index;
    placement.position = {at[0], at[1], at[2]};
    placement.rotation = {turn[0], turn[1], turn[2]};
    // Through the panel's own rule, so a hand-edited zero or negative scale
    // is clamped rather than turning the model inside out.
    placement.scale = normalizeEditorPropertyValue(
        EditorPropertyField::SCALE, readNumber(entry, "scale", 1.0f));
    // A prop written before collision existed has no flag, and reads as
    // solid — the default a dropped one gets.
    placement.collides = readBool(entry, "collides").value_or(true);
    placement.animation = readString(entry, "animation");
    readActor(entry, placement);
    placement.id = readString(entry, "id");
    if (placement.id.empty()) {
      placement.id = mintEditorPlacementId(document, assets[index]);
    }
    return placement;
  }

  /// Everything about a light but where it stands: what it throws, how
  /// hard, in what colour, how far. Each falls back to the value a dropped
  /// light of its kind already has.
  void readLightShading(const json& entry, EditorLight& light) {
    const Triple aim =
        readTriple(entry, "direction",
                   {light.direction.x, light.direction.y, light.direction.z});
    const Triple tint = readTriple(
        entry, "color", {light.color.x, light.color.y, light.color.z});
    light.direction = {aim[0], aim[1], aim[2]};
    light.color = {tint[0], tint[1], tint[2]};
    light.intensity = readNumber(entry, "intensity", light.intensity);
    light.range = readNumber(entry, "range", light.range);
  }

  EditorLight readLight(const json& entry, const EditorDocument& document) {
    const Triple at = readTriple(entry, "at", ZERO_TRIPLE);
    EditorLight light =
        makeEditorLight(editorLightKindFromId(readString(entry, "kind")),
                        {at[0], at[1], at[2]});
    readLightShading(entry, light);
    light.id = readString(entry, "id");
    if (light.id.empty()) {
      light.id = mintEditorLightId(document, light.kind);
    }
    return light;
  }

  /// @p character as the qualified reference the editor holds and writes:
  /// a hand-written bare id, `scout`, reads as `character:scout`. Anything
  /// already qualified is kept as written, whether or not the project has
  /// that character — the table is read separately, and may be fixed.
  std::string characterRef(const std::string& character) {
    return character.empty() || character.contains(':')
               ? character
               : editorCharacterRef(character);
  }

  /// One player start. Its player is read from the property block and held
  /// to a slot a session has, so a hand-edited `"player": 9` opens as
  /// player 4 rather than as a start nobody spawns at.
  EditorPlayerStart readPlayerStart(const json& entry,
                                    const EditorDocument& document) {
    const Triple at = readTriple(entry, "at", ZERO_TRIPLE);
    const json properties = entry.value("properties", json::object());
    EditorPlayerStart start = makeEditorPlayerStart(
        clampEditorPlayerSlot(readNumber(properties, "player", 1.0f)),
        {at[0], at[1], at[2]});
    start.character = characterRef(readString(properties, "character"));
    start.id = readString(entry, "id");
    if (start.id.empty()) {
      start.id = mintEditorPlayerStartId(document);
    }
    return start;
  }

  /// One waypoint, its route and place held to what a level can hold.
  EditorWaypoint readWaypoint(const json& entry,
                              const EditorDocument& document) {
    const Triple at = readTriple(entry, "at", ZERO_TRIPLE);
    const json properties = entry.value("properties", json::object());
    EditorWaypoint waypoint = makeEditorWaypoint(
        clampEditorRoute(readNumber(properties, "route", 1.0f)),
        clampEditorWaypointOrder(readNumber(properties, "order", 1.0f)),
        {at[0], at[1], at[2]});
    waypoint.id = readString(entry, "id");
    if (waypoint.id.empty()) {
      waypoint.id = mintEditorWaypointId(document);
    }
    return waypoint;
  }

  /// The array under @p key, or an empty one when the file has no such
  /// array. A level with no lights in it is an ordinary level.
  json arrayAt(const json& content, const char* key) {
    const auto found = content.find(key);
    return found != content.end() && found->is_array() ? *found : json::array();
  }

  void readProps(const json& content, const std::vector<EditorAsset>& assets,
                 EditorLevelLoad& load) {
    const std::unordered_map<std::string, size_t> by_ref =
        assetsByReference(assets);
    for (const json& entry : arrayAt(content, "props")) {
      const auto found = entry.is_object()
                             ? by_ref.find(readString(entry, "asset"))
                             : by_ref.end();
      if (found == by_ref.end()) {
        ++load.dropped_props;
        continue;
      }
      load.document.placements.push_back(
          readProp(entry, found->second, load.document, assets));
    }
  }

  /// Every entity the editor has a definition for: player starts and
  /// waypoints. Any other is dropped and counted, as a prop naming a
  /// missing asset is, rather than silently rewritten into something else.
  void readEntities(const json& content, EditorLevelLoad& load) {
    for (const json& entry : arrayAt(content, "entities")) {
      const std::string definition =
          entry.is_object() ? readString(entry, "definition") : std::string{};
      if (definition == EDITOR_PLAYER_START_DEFINITION) {
        load.document.player_starts.push_back(
            readPlayerStart(entry, load.document));
      } else if (definition == EDITOR_WAYPOINT_DEFINITION) {
        load.document.waypoints.push_back(readWaypoint(entry, load.document));
      } else {
        ++load.dropped_entities;
      }
    }
  }

  void readLights(const json& content, EditorDocument& document) {
    for (const json& entry : arrayAt(content, "lights")) {
      if (entry.is_object()) {
        document.lights.push_back(readLight(entry, document));
      }
    }
  }

}  // namespace

std::string serializeEditorLevel(const EditorDocument& document,
                                 const std::vector<EditorAsset>& assets,
                                 std::string_view id) {
  json content;
  content["props"] = propsJson(document, assets);
  content["lights"] = lightsJson(document);
  content["entities"] = entitiesJson(document);
  json out;
  out["schema"] = EDITOR_LEVEL_SCHEMA;
  out["id"] = std::string(id);
  out["name"] = std::string(id);
  out["content"] = std::move(content);
  return out.dump(2);
}

std::optional<EditorLevelLoad>
parseEditorLevel(std::string_view text,
                 const std::vector<EditorAsset>& assets) {
  const std::optional<json> parsed = tryParse(text);
  if (!parsed || !parsed->is_object()) {
    return std::nullopt;
  }
  if (readString(*parsed, "schema") != EDITOR_LEVEL_SCHEMA) {
    return std::nullopt;
  }
  const json content = parsed->value("content", json::object());
  EditorLevelLoad load;
  readProps(content, assets, load);
  readLights(content, load.document);
  readEntities(content, load);
  return load;
}

}  // namespace eng::editor
