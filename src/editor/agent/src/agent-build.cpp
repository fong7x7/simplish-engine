#include "agent-build.h"

#include "agent-call.h"

#include <editor/agent/agent-names.h>
#include <editor/build/editor-toolchain.h>
#include <editor/project/project-paths.h>
#include <game/logic/game-logic-entry.h>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// @p path as a string, or null when it is empty.
  json pathOrNull(const std::filesystem::path& path) {
    return path.empty() ? json(nullptr) : json(path.string());
  }

  /// The last build: what it made, how it went, and what it printed.
  json buildJson(const EditorBuildState& build) {
    return {{"kind", agentBuildKindName(build.kind)},
            {"status", agentBuildStatusName(build.status)},
            {"builds", build.builds},
            {"log", pathOrNull(build.log)},
            {"errors", build.errors},
            {"log_tail", build.log_tail}};
  }

  /// What the editor builds with.
  json toolchainJson() {
    const EditorToolchain tools = editorToolchain();
    return {{"cmake", tools.cmake},
            {"generator", tools.generator},
            {"cxx_compiler", tools.cxx_compiler},
            {"engine_root", tools.engine_root.string()}};
  }

}  // namespace

std::string agentBuildJson(const EditorShellState& state) {
  const EditorBuildState& build = state.build;
  const bool open = state.project.loaded;
  return json{
      {"has_logic", build.has_logic},
      {"source", open ? json(projectSourcePath(state.project.root).string())
                      : json(nullptr)},
      {"logic_loaded", build.logic_loaded},
      {"logic_stale", build.logic_stale},
      {"logic_error", build.logic_error},
      {"logic_library", pathOrNull(build.logic_library)},
      {"api_version", game::GAME_LOGIC_API_VERSION},
      {"deployed", pathOrNull(build.deployed)},
      {"toolchain", toolchainJson()},
      {"build", buildJson(build)}}
      .dump(2);
}

AgentResult runAgentGetBuild(EditorShellState& state,
                             const nlohmann::json& /*params*/) {
  return agentOk(agentBuildJson(state));
}

}  // namespace eng::editor
