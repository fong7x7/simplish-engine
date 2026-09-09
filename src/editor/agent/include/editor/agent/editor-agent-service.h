#pragma once

/// @file editor-agent-service.h
/// @brief Binds the agent tool surface to a running editor and a port.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <editor/agent/agent-result.h>
#include <editor/shell/simplish-editor.h>
#include <engine/agent/agent-http-request.h>
#include <engine/agent/agent-http-response.h>
#include <engine/agent/local-agent-server.h>
#include <string>

namespace eng::editor {

/// The port an agent reaches this editor on when nothing says otherwise.
///
/// Nothing opens it by default: the editor listens only when it is told a
/// port to listen on. A local port that edits the user's project is not
/// something to switch on for everybody who launches the editor.
inline constexpr uint16_t EDITOR_AGENT_DEFAULT_PORT = 8787;

/// The port `SIMPLISH_AGENT_PORT` asks for, or zero when it is unset,
/// empty, not a number, or out of range — every one of which means "do not
/// open a port", because a typo should not silently open a different one.
///
/// The value `default` asks for `EDITOR_AGENT_DEFAULT_PORT`, so that
/// turning the API on does not mean remembering a number.
[[nodiscard]] uint16_t editorAgentPortFromEnvironment();

/// Serves the agent API for one editor.
///
/// Owns the socket, installs itself as that editor's per-tick state hook,
/// and turns each HTTP request into one tool call. Everything it does
/// happens on the tick that drained it, so an agent's edit lands in the
/// same document a person at the window is editing, in the frame it
/// arrived, with no locking anywhere.
/// @thread_safety Main-thread-only.
class EditorAgentService {
public:
  EditorAgentService() = default;
  ~EditorAgentService() = default;
  EditorAgentService(const EditorAgentService&) = delete;
  EditorAgentService& operator=(const EditorAgentService&) = delete;
  EditorAgentService(EditorAgentService&&) = delete;
  EditorAgentService& operator=(EditorAgentService&&) = delete;

  /// Listen on @p port and drive @p editor from what arrives there. False
  /// when the port cannot be bound, which leaves the editor running
  /// without an agent API rather than not running.
  bool attach(SimplishEditor& editor, uint16_t port);

  /// Stop listening. The editor keeps running.
  void detach();

  /// Whether the service is listening.
  [[nodiscard]] bool isOpen() const;

  /// The port it is listening on, or zero.
  [[nodiscard]] uint16_t port() const;

  /// Answer one request against @p state, without any socket involved.
  ///
  /// The routing table, separated from the transport so a test can post to
  /// it directly — which is how every route here is covered without
  /// opening a port.
  [[nodiscard]] agent::AgentHttpResponse
  route(EditorShellState& state, const agent::AgentHttpRequest& request);

private:
  /// Answer one `GET`: the routes that only read.
  [[nodiscard]] agent::AgentHttpResponse routeGet(EditorShellState& state,
                                                  const std::string& path);
  /// Answer one `POST`: the routes that call a tool.
  [[nodiscard]] agent::AgentHttpResponse
  routePost(EditorShellState& state, const agent::AgentHttpRequest& request);
  /// Drain whatever has arrived and run it. Returns whether the editor's
  /// state changed, which is what the hook's caller rebuilds the chrome on.
  bool pump(EditorShellState& state);
  /// Turn one tool result into a response, and carry out whatever it left
  /// for the editor to do.
  [[nodiscard]] agent::AgentHttpResponse finish(const AgentResult& result);
  /// Carry out the work a tool could not do itself.
  void runHostRequest(const AgentHostRequest& request);

  /// The socket, or a closed server when nothing is attached.
  agent::LocalAgentServer server_;
  /// The editor being driven, or nullptr when nothing is attached.
  SimplishEditor* editor_ = nullptr;
  /// Whether anything this poll ran changed the editor's state.
  bool changed_ = false;
};

}  // namespace eng::editor
