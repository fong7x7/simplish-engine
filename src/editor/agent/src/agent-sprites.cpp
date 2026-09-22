#include "agent-sprites.h"

#include "agent-call.h"
#include "agent-json-values.h"

#include <algorithm>
#include <editor/agent/agent-names.h>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-entity-id.h>
#include <editor/shell/editor-sprite-ops.h>
#include <optional>
#include <string>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// What `add_sprite` says when it is called wrongly.
  constexpr std::string_view ADD_SPRITE_USAGE =
      "x and y are required; sheet, when given, is a path list_sprites "
      "lists under sheets";

  /// What `set_sheet` says when it is called wrongly.
  constexpr std::string_view SET_SHEET_USAGE =
      "expected target \"sprite\" with an index, or \"selection\" when a "
      "billboard is selected, and sheet: a path that list_sprites lists "
      "under sheets";

  /// Record a changed billboard as one undoable edit, and select it.
  AgentResult recordSprite(EditorShellState& state, size_t index,
                           const EditorSprite& prior,
                           const EditorSprite& next) {
    performEditorAction(state.history, state.document,
                        {.kind = EditorActionKind::TRANSFORM_SPRITE,
                         .index = index,
                         .sprite = next,
                         .sprite_prior = prior});
    state.selection = {EditorSelectionKind::SPRITE, index};
    return agentEdited(agentSpritePayload(state, index));
  }

  /// Add @p sprite to the document as one undoable edit, and select it.
  AgentResult addSprite(EditorShellState& state, EditorSprite sprite) {
    const size_t index = state.document.sprites.size();
    sprite.id = mintEditorSpriteId(state.document);
    performEditorAction(state.history, state.document,
                        {.kind = EditorActionKind::ADD_SPRITE,
                         .index = index,
                         .sprite = sprite});
    state.selection = {EditorSelectionKind::SPRITE, index};
    return agentEdited(agentSpritePayload(state, index));
  }

  /// The billboard a `set_sheet` call names: the selected one, or the one
  /// at its index. Nothing when it names none that is there.
  std::optional<size_t> spriteTarget(const EditorShellState& state,
                                     const json& params) {
    const std::string target = agentStringParam(params, "target").value_or("");
    std::optional<size_t> index;
    if (target == "selection" &&
        state.selection.kind == EditorSelectionKind::SPRITE) {
      index = state.selection.index;
    } else if (target == "sprite") {
      index = agentIndexParam(params, "index");
    }
    return index && *index < state.document.sprites.size() ? index
                                                           : std::nullopt;
  }

  /// Whether the project holds a sheet at @p path.
  bool hasSheet(const EditorShellState& state, const std::string& path) {
    return std::ranges::any_of(state.sheets, [&path](const auto& sheet) {
      return sheet.generic_string() == path;
    });
  }

  /// The sheet a new billboard shows: the one asked for, the project's
  /// first when none was, and nothing at all when the project has none.
  std::string startingSheet(const EditorShellState& state, const json& params) {
    if (const std::optional<std::string> asked =
            agentStringParam(params, "sheet")) {
      return *asked;
    }
    return state.sheets.empty() ? std::string{}
                                : state.sheets.front().generic_string();
  }

  /// The numbers `add_sprite` takes beside the position, which arrives as
  /// x, y and z. In the order the panel lists them, and read under the
  /// names `set_property` writes them by.
  constexpr EditorPropertyField SPRITE_NUMBER_FIELDS[] = {
      EditorPropertyField::HEIGHT, EditorPropertyField::COLUMNS,
      EditorPropertyField::ROWS,   EditorPropertyField::FRAMES,
      EditorPropertyField::FPS,
  };

  /// Every one of those the call gave, written through the panel's own
  /// rule so an `add_sprite` is held to what the panel would allow. The
  /// grid before the frame count, which is the order it has to be in for
  /// the count to be held to a grid already set.
  void applySpriteParams(EditorSprite& sprite, const json& params) {
    for (const EditorPropertyField field : SPRITE_NUMBER_FIELDS) {
      const std::optional<double> given =
          agentNumberParam(params, agentPropertyFieldName(field));
      if (given) {
        setEditorSpriteValue(sprite, field, static_cast<float>(*given));
      }
    }
  }

}  // namespace

AgentResult runAgentAddSprite(EditorShellState& state, const json& params) {
  const std::optional<double> x = agentNumberParam(params, "x");
  const std::optional<double> y = agentNumberParam(params, "y");
  const std::string sheet = startingSheet(state, params);
  if (!x || !y || (!sheet.empty() && !hasSheet(state, sheet))) {
    return agentFailure(AgentStatus::BAD_PARAMS, ADD_SPRITE_USAGE);
  }
  const WorldPoint at{static_cast<float>(*x), static_cast<float>(*y),
                      agentFloatParam(params, "z", EDITOR_SPRITE_DROP_HEIGHT)};
  EditorSprite sprite = makeEditorSprite(sheet, at);
  applySpriteParams(sprite, params);
  return addSprite(state, sprite);
}

AgentResult runAgentSetSheet(EditorShellState& state, const json& params) {
  const std::optional<size_t> index = spriteTarget(state, params);
  const std::string sheet = agentStringParam(params, "sheet").value_or("");
  if (!index || !hasSheet(state, sheet)) {
    return agentFailure(AgentStatus::BAD_PARAMS, SET_SHEET_USAGE);
  }
  const EditorSprite prior = state.document.sprites[*index];
  EditorSprite next = prior;
  next.sheet = sheet;
  if (sameEditorSprite(prior, next)) {
    return agentOk(agentSpritePayload(state, *index));
  }
  return recordSprite(state, *index, prior, next);
}

AgentResult setAgentSpriteField(EditorShellState& state, size_t index,
                                EditorPropertyField field, float value) {
  if (!editorSpriteHasField(field)) {
    return agentFailure(AgentStatus::BAD_PARAMS,
                        "a sprite billboard holds a position, a height, and "
                        "the grid and speed of its sheet; get_selection "
                        "lists every field it has");
  }
  const EditorSprite prior = state.document.sprites[index];
  EditorSprite next = prior;
  setEditorSpriteValue(next, field, value);
  if (sameEditorSprite(prior, next)) {
    return agentOk(agentSpritePayload(state, index));
  }
  return recordSprite(state, index, prior, next);
}

AgentResult translateAgentSprite(EditorShellState& state, size_t index,
                                 const json& params) {
  const EditorSprite prior = state.document.sprites[index];
  EditorSprite next = prior;
  next.position.x += agentFloatParam(params, "dx", 0.0f);
  next.position.y += agentFloatParam(params, "dy", 0.0f);
  next.position.z += agentFloatParam(params, "dz", 0.0f);
  if (sameEditorSprite(prior, next)) {
    return agentOk(agentSpritePayload(state, index));
  }
  return recordSprite(state, index, prior, next);
}

std::string agentSpritePayload(const EditorShellState& state, size_t index) {
  json out = agentSpriteValue(state.document.sprites[index]);
  out["index"] = index;
  return out.dump(2);
}

json agentSpriteSheetsJson(const EditorShellState& state) {
  json sheets = json::array();
  for (const std::filesystem::path& sheet : state.sheets) {
    sheets.push_back(sheet.generic_string());
  }
  return sheets;
}

}  // namespace eng::editor
