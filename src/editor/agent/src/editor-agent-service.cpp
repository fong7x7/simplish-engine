#include "agent-call.h"

#include <cstdlib>
#include <editor/agent/agent-dispatch.h>
#include <editor/agent/agent-state-json.h>
#include <editor/agent/editor-agent-service.h>
#include <engine/core/logger.h>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace eng::editor {

namespace {

  /// Where a call to one named tool arrives.
  constexpr std::string_view TOOL_ROUTE = "/tools/";

  /// What an unrouted request is told, so a caller that guessed a path is
  /// pointed at the ones that exist rather than at a bare 404.
  constexpr std::string_view ROUTE_HELP =
      "unknown path; GET / describes this editor, GET /tools lists its "
      "tools, GET /state reports its state, POST /call takes "
      "{\"tool\": \"...\", \"params\": {...}}, and POST /tools/<name> takes "
      "the params alone";

  /// A response carrying @p body as it stands.
  agent::AgentHttpResponse jsonResponse(std::string body) {
    return {200, std::move(body)};
  }

}  // namespace

uint16_t editorAgentPortFromEnvironment() {
  // NOLINTNEXTLINE(concurrency-mt-unsafe) -- main-thread-only startup read
  const char* value = std::getenv("SIMPLISH_AGENT_PORT");
  if (value == nullptr || *value == '\0') {
    return 0;
  }
  if (std::string_view(value) == "default") {
    return EDITOR_AGENT_DEFAULT_PORT;
  }
  const long parsed = std::strtol(value, nullptr, 10);
  if (parsed <= 0 || parsed > 65535) {
    return 0;
  }
  return static_cast<uint16_t>(parsed);
}

bool EditorAgentService::attach(SimplishEditor& editor, uint16_t port) {
  if (!server_.open(port)) {
    LOG_ERROR("editor",
              "Agent API could not bind port " + std::to_string(port));
    return false;
  }
  editor_ = &editor;
  editor.setStateHook([this](EditorShellState& state) { return pump(state); });
  LOG_INFO("editor", "Agent API listening on http://127.0.0.1:" +
                         std::to_string(server_.port()));
  return true;
}

void EditorAgentService::detach() {
  if (editor_ != nullptr) {
    editor_->setStateHook({});
    editor_ = nullptr;
  }
  server_.close();
}

bool EditorAgentService::isOpen() const {
  return server_.isOpen();
}

uint16_t EditorAgentService::port() const {
  return server_.port();
}

bool EditorAgentService::runLevelRequest(const AgentHostRequest& request) {
  if (request.kind == AgentHostRequestKind::CREATE_LEVEL) {
    editor_->createLevel(request.level, request.unsaved);
  } else if (request.kind == AgentHostRequestKind::OPEN_LEVEL) {
    editor_->openLevel(request.level, request.unsaved);
  } else {
    return false;
  }
  return true;
}

void EditorAgentService::runProjectRequest(const AgentHostRequest& request) {
  switch (request.kind) {
    case AgentHostRequestKind::RUN_COMMAND:
      editor_->runMenuCommand(request.command);
      break;
    case AgentHostRequestKind::OPEN_PROJECT:
      (void)editor_->openProjectAt(std::filesystem::path(request.path));
      break;
    case AgentHostRequestKind::RESCAN_ASSETS:
      editor_->rescanAssets();
      break;
    // Nothing to do, or already done by `runLevelRequest`. Listed rather
    // than defaulted, so a kind added to the enum fails the build here.
    case AgentHostRequestKind::NONE:
    case AgentHostRequestKind::CREATE_LEVEL:
    case AgentHostRequestKind::OPEN_LEVEL:
      break;
  }
}

void EditorAgentService::runHostRequest(const AgentHostRequest& request) {
  if (editor_ == nullptr || runLevelRequest(request)) {
    return;
  }
  runProjectRequest(request);
}

agent::AgentHttpResponse EditorAgentService::finish(const AgentResult& result) {
  changed_ = changed_ || result.changed;
  runHostRequest(result.host);
  return jsonResponse(agentResponseJson(result));
}

agent::AgentHttpResponse EditorAgentService::routeGet(EditorShellState& state,
                                                      const std::string& path) {
  if (path == "/") {
    return jsonResponse(agentDescribeJson(state));
  }
  if (path == "/tools") {
    return jsonResponse(agentManifestJson());
  }
  if (path == "/state") {
    return jsonResponse(agentStateJson(state));
  }
  return {404, nlohmann::json{{"message", ROUTE_HELP}}.dump(2)};
}

agent::AgentHttpResponse
EditorAgentService::routePost(EditorShellState& state,
                              const agent::AgentHttpRequest& request) {
  if (request.path == "/call") {
    return finish(runAgentRequest(state, request.body));
  }
  if (request.path.starts_with(TOOL_ROUTE)) {
    const std::string_view tool =
        std::string_view(request.path).substr(TOOL_ROUTE.size());
    return finish(runAgentTool(state, tool, request.body));
  }
  return {404, nlohmann::json{{"message", ROUTE_HELP}}.dump(2)};
}

agent::AgentHttpResponse
EditorAgentService::route(EditorShellState& state,
                          const agent::AgentHttpRequest& request) {
  if (request.method == "GET") {
    return routeGet(state, request.path);
  }
  if (request.method == "POST") {
    return routePost(state, request);
  }
  return {404, nlohmann::json{{"message", ROUTE_HELP}}.dump(2)};
}

bool EditorAgentService::pump(EditorShellState& state) {
  changed_ = false;
  server_.poll([this, &state](const agent::AgentHttpRequest& request) {
    return route(state, request);
  });
  return changed_;
}

}  // namespace eng::editor
