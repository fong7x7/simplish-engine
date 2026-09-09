#include <catch2/catch_test_macros.hpp>
#include <editor/agent/agent-dispatch.h>
#include <editor/agent/agent-names.h>
#include <editor/agent/agent-state-json.h>
#include <editor/agent/agent-tool-info.h>
#include <nlohmann/json.hpp>
#include <set>
#include <string>

using nlohmann::json;
using namespace eng;
using namespace eng::editor;

// The tests here are the guarantee the agent API rests on: that every tool,
// command, property and toolbar tool the editor has is reachable from the
// outside. The MCP bridge builds its own tool list from the manifest these
// check, so a gap here is a gap an agent would silently never see.

TEST_CASE("every tool is named, described, and answered") {
  std::set<std::string> names;
  for (const AgentTool tool : AGENT_TOOLS) {
    const AgentToolInfo& info = agentToolInfo(tool);
    INFO("tool " << info.name);
    REQUIRE(info.tool == tool);
    REQUIRE_FALSE(info.name.empty());
    REQUIRE_FALSE(info.summary.empty());
    REQUIRE(names.insert(std::string(info.name)).second);
  }
}

TEST_CASE("every tool name dispatches to something") {
  EditorShellState state;
  for (const AgentTool tool : AGENT_TOOLS) {
    const AgentResult result = runAgentTool(state, agentToolName(tool), "{}");
    INFO("tool " << agentToolName(tool));
    // Whatever it makes of an empty call, no tool may be unreachable.
    REQUIRE(result.status != AgentStatus::UNKNOWN_TOOL);
  }
}

TEST_CASE("every tool parameter is named and explained") {
  for (const AgentTool tool : AGENT_TOOLS) {
    for (const AgentParam& param : agentToolInfo(tool).params) {
      INFO("tool " << agentToolName(tool) << " param " << param.name);
      REQUIRE_FALSE(param.name.empty());
      REQUIRE_FALSE(param.description.empty());
    }
  }
}

TEST_CASE("the manifest publishes every tool the editor has") {
  const json manifest = json::parse(agentManifestJson());

  REQUIRE(manifest.at("tools").size() == std::size(AGENT_TOOLS));
  for (const AgentTool tool : AGENT_TOOLS) {
    const auto listed =
        std::find_if(manifest.at("tools").begin(), manifest.at("tools").end(),
                     [tool](const json& entry) {
                       return entry.at("name") == agentToolName(tool);
                     });
    INFO("tool " << agentToolName(tool));
    REQUIRE(listed != manifest.at("tools").end());
  }
}

TEST_CASE("every property field has a name that round-trips") {
  std::set<std::string> names;
  for (const EditorPropertyField field : EDITOR_ALL_PROPERTY_FIELDS) {
    const std::string_view name = agentPropertyFieldName(field);
    INFO("field " << name);
    REQUIRE_FALSE(name.empty());
    REQUIRE(names.insert(std::string(name)).second);
    REQUIRE(findAgentPropertyField(name) == field);
  }
}

TEST_CASE("every menu command has a name that round-trips") {
  std::set<std::string> names;
  for (const EditorMenuCommandInfo& info : EDITOR_MENU_COMMAND_INFO) {
    if (info.command == EditorMenuCommand::SEPARATOR) {
      continue;
    }
    const std::string_view name = agentMenuCommandName(info.command);
    INFO("command " << info.label);
    REQUIRE_FALSE(name.empty());
    REQUIRE(names.insert(std::string(name)).second);
    REQUIRE(findAgentMenuCommand(name) == info.command);
  }
}

TEST_CASE("the separator is not a command an agent can run") {
  REQUIRE(agentMenuCommandName(EditorMenuCommand::SEPARATOR).empty());
  REQUIRE_FALSE(findAgentMenuCommand("").has_value());
}

TEST_CASE("every toolbar tool has a name that round-trips") {
  for (const EditorTool tool : EDITOR_TOOLS) {
    const std::string_view name = agentEditorToolName(tool);
    INFO("tool " << name);
    REQUIRE_FALSE(name.empty());
    REQUIRE(findAgentEditorTool(name) == tool);
  }
}

TEST_CASE("list_commands reports every command the menu bar has") {
  EditorShellState state;
  const json listed = json::parse(agentCommandsJson(state));

  REQUIRE(listed.at("commands").size() ==
          std::size(EDITOR_MENU_COMMAND_INFO) - 1);
}
