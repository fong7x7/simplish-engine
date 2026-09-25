#include "agent-ui.h"

#include "agent-call.h"

#include <algorithm>
#include <game/ui/ui-actions.h>
#include <game/ui/ui-render-size.h>
#include <game/ui/ui-screen-json.h>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// The word each layer is published as, in `UiScreenLayer` order.
  constexpr std::string_view LAYERS[] = {"menu", "hud"};

  /// @p rect as `[x, y, w, h]`.
  json rectJson(const Rect& rect) {
    return json::array({rect.x, rect.y, rect.w, rect.h});
  }

  /// Every button @p node and its children hold, into @p out.
  // NOLINTNEXTLINE(misc-no-recursion) -- a screen is a tree, bounded in depth
  void buttonsOf(const game::UiNode& node, json& out) {
    if (node.kind == game::UiNodeKind::BUTTON) {
      out.push_back(
          {{"id", node.id}, {"action", node.action}, {"text", node.text}});
    }
    for (const game::UiNode& child : node.children) {
      buttonsOf(child, out);
    }
  }

  /// @p screen as `get_ui_screens` lists it.
  json screenJson(const game::UiScreen& screen) {
    json buttons = json::array();
    buttonsOf(screen.root, buttons);
    return {{"id", screen.id},
            {"layer", LAYERS[static_cast<size_t>(screen.layer)]},
            {"buttons", buttons}};
  }

  /// The problems `text`, a screen named @p id, has; and whether it is a
  /// screen at all.
  game::UiScreenRead checkScreen(const json& screen, const std::string& id) {
    return screen.is_object()
               ? game::parseUiScreen(screen.dump(), id)
               : game::UiScreenRead{std::nullopt, {"screen must be an object"}};
  }

  /// Why `set_ui_screen` refused a screen that read as @p read.
  std::string refusal(const game::UiScreenRead& read) {
    std::string message = "id must be lowercase letters, digits, _ and -, "
                          "and screen a screen";
    for (const std::string& problem : read.problems) {
      message += "; " + problem;
    }
    return message;
  }

  /// The size a render asks for, within the bounds.
  game::UiRenderSize renderSize(const json& params) {
    const auto side = [&params](std::string_view key, uint32_t fallback) {
      const auto value = agentIndexParam(params, key);
      return value ? static_cast<uint32_t>(std::clamp<size_t>(
                         *value, 16, game::UI_RENDER_MAX_SIDE))
                   : fallback;
    };
    return {side("width", 1280), side("height", 720)};
  }

}  // namespace

std::string agentUiScreensJson(const EditorShellState& state) {
  json screens = json::array();
  for (const game::UiScreen& screen : state.ui.screens) {
    screens.push_back(screenJson(screen));
  }
  return json{{"screens", screens},
              {"actions", game::uiActions(state.ui.screens)},
              {"problems", state.ui.problems}}
      .dump(2);
}

AgentResult runAgentGetUiScreens(EditorShellState& state, const json&) {
  return agentOk(agentUiScreensJson(state));
}

AgentResult runAgentSetUiScreen(EditorShellState& state, const json& params) {
  const std::string id = agentStringParam(params, "id").value_or("");
  if (!state.project.loaded) {
    return agentFailure(AgentStatus::UNAVAILABLE, "no project is open");
  }
  const game::UiScreenRead read =
      checkScreen(params.value("screen", json()), id);
  if (!editorUiScreenIdValid(id) || !read.screen) {
    return agentFailure(AgentStatus::BAD_PARAMS, refusal(read));
  }
  AgentResult result = agentOk(json{{"queued", "write " + id}}.dump());
  result.host.kind = AgentHostRequestKind::WRITE_UI_SCREEN;
  result.host.name = id;
  result.host.text = params["screen"].dump(2);
  return result;
}

AgentResult runAgentRenderUiScreen(EditorShellState& state,
                                   const json& params) {
  const std::string id = agentStringParam(params, "id").value_or("");
  if (findEditorUiScreen(state.ui, id) == nullptr) {
    return agentFailure(AgentStatus::NOT_FOUND,
                        "id must be one of get_ui_screens' screens");
  }
  const game::UiRenderSize size = renderSize(params);
  AgentResult result = agentOk(json{{"queued", "render " + id}}.dump());
  result.host.kind = AgentHostRequestKind::RENDER_UI_SCREEN;
  result.host.name = id;
  result.host.width = size.width;
  result.host.height = size.height;
  result.host.text = params.value("values", json::object()).dump();
  return result;
}

std::string agentUiRenderJson(const EditorShellState& state) {
  const EditorUiRender& render = state.ui_render;
  json buttons = json::array();
  for (const game::UiButtonInfo& button : render.buttons) {
    buttons.push_back({{"id", button.id},
                       {"action", button.action},
                       {"text", button.text},
                       {"rect", rectJson(button.rect)}});
  }
  return json{{"id", render.id},           {"image", render.path},
              {"width", render.width},     {"height", render.height},
              {"text_drawn", render.text}, {"buttons", buttons},
              {"error", render.error}}
      .dump(2);
}

AgentResult runAgentPressUi(EditorShellState& state, const json& params) {
  const std::string action = agentStringParam(params, "action").value_or("");
  const std::vector<std::string> actions = game::uiActions(state.ui.screens);
  if (state.playtest.mode != EditorPlayMode::PLAYING) {
    return agentFailure(AgentStatus::UNAVAILABLE, "nothing is playing");
  }
  if (std::ranges::find(actions, action) == actions.end()) {
    return agentFailure(AgentStatus::NOT_FOUND,
                        "action must be one of get_ui_screens' actions");
  }
  state.playtest.ui.press = action;
  return agentEdited(json{{"pressed", action}}.dump());
}

json agentPlaytestUiJson(const EditorPlaytestUi& ui) {
  json buttons = json::array();
  for (const EditorPlaytestButton& button : ui.buttons) {
    buttons.push_back({{"screen", button.screen},
                       {"id", button.id},
                       {"action", button.action},
                       {"text", button.text},
                       {"rect", rectJson(button.rect)}});
  }
  return {{"open", ui.open}, {"values", ui.values}, {"buttons", buttons}};
}

}  // namespace eng::editor
