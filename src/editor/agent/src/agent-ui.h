#pragma once

/// @file agent-ui.h
/// @brief The agent tools for a game's own screens: menus and a HUD.
/// @par Threading
/// Main-thread-only, with the editor state they read and change.

#include <editor/agent/agent-result.h>
#include <editor/shell/editor-playtest-ui.h>
#include <editor/shell/editor-shell-state.h>
#include <nlohmann/json.hpp>
#include <string>

namespace eng::editor {

/// `get_ui_screens`: every screen of the project, its buttons and actions,
/// and what is wrong with any screen file.
[[nodiscard]] AgentResult runAgentGetUiScreens(EditorShellState& state,
                                               const nlohmann::json& params);

/// `set_ui_screen`: check a screen, then have the editor write it.
[[nodiscard]] AgentResult runAgentSetUiScreen(EditorShellState& state,
                                              const nlohmann::json& params);

/// `render_ui_screen`: have the editor draw a screen to a PNG.
[[nodiscard]] AgentResult runAgentRenderUiScreen(EditorShellState& state,
                                                 const nlohmann::json& params);

/// `press_ui`: have player 1 choose an action on the next tick.
[[nodiscard]] AgentResult runAgentPressUi(EditorShellState& state,
                                          const nlohmann::json& params);

/// What `get_ui_screens` answers.
[[nodiscard]] std::string agentUiScreensJson(const EditorShellState& state);

/// What `render_ui_screen` answers, once the editor has drawn it.
[[nodiscard]] std::string agentUiRenderJson(const EditorShellState& state);

/// The game screens a playtest shows, as `get_playtest` reports them.
[[nodiscard]] nlohmann::json agentPlaytestUiJson(const EditorPlaytestUi& ui);

}  // namespace eng::editor
