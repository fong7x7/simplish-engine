#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <editor/agent/editor-agent-service.h>
#include <nlohmann/json.hpp>

using nlohmann::json;
using namespace eng;
using namespace eng::agent;
using namespace eng::editor;

namespace {

/// Post @p body to @p path, without a socket anywhere.
AgentHttpResponse post(EditorAgentService& service, EditorShellState& state,
                       std::string_view path, std::string_view body) {
  return service.route(state, {"POST", std::string(path), std::string(body)});
}

}  // namespace

TEST_CASE("the root path describes the editor") {
  EditorAgentService service;
  EditorShellState state;

  const AgentHttpResponse response = service.route(state, {"GET", "/", ""});

  REQUIRE(response.status == 200);
  REQUIRE(json::parse(response.body).at("api") == "simplish-editor");
}

TEST_CASE("the tools path serves the manifest an agent builds its list from") {
  EditorAgentService service;
  EditorShellState state;

  const json manifest =
      json::parse(service.route(state, {"GET", "/tools", ""}).body);

  REQUIRE_FALSE(manifest.at("tools").empty());
  REQUIRE(manifest.at("tools").at(0).contains("params"));
}

TEST_CASE("a call posts a tool and its params together") {
  EditorAgentService service;
  EditorShellState state;

  const AgentHttpResponse response =
      post(service, state, "/call",
           R"({"tool": "add_light", "params": {"kind": "point", "x": 1,
                                               "y": 1}})");

  REQUIRE(json::parse(response.body).at("status") == "ok");
  REQUIRE(state.document.lights.size() == 1);
}

TEST_CASE("a tool can also be posted by name, with the params alone") {
  EditorAgentService service;
  EditorShellState state;

  const AgentHttpResponse response =
      post(service, state, "/tools/add_light",
           R"({"kind": "directional", "x": 0, "y": 0})");

  REQUIRE(json::parse(response.body).at("status") == "ok");
  REQUIRE(state.document.lights.size() == 1);
}

TEST_CASE("a path nobody serves says which ones are served") {
  EditorAgentService service;
  EditorShellState state;

  const AgentHttpResponse response =
      service.route(state, {"GET", "/levels", ""});

  REQUIRE(response.status == 404);
  REQUIRE_FALSE(
      json::parse(response.body).at("message").get<std::string>().empty());
}

TEST_CASE("a host request with no editor attached is dropped, not crashed") {
  EditorAgentService service;
  EditorShellState state;

  const AgentHttpResponse response =
      post(service, state, "/tools/run_command", R"({"command": "about"})");

  REQUIRE(json::parse(response.body).at("status") == "ok");
}

TEST_CASE("no port is opened unless the environment names one") {
  // NOLINTNEXTLINE(concurrency-mt-unsafe) -- single-threaded test
  unsetenv("SIMPLISH_AGENT_PORT");
  REQUIRE(editorAgentPortFromEnvironment() == 0);

  // NOLINTNEXTLINE(concurrency-mt-unsafe) -- single-threaded test
  setenv("SIMPLISH_AGENT_PORT", "not-a-port", 1);
  REQUIRE(editorAgentPortFromEnvironment() == 0);

  // NOLINTNEXTLINE(concurrency-mt-unsafe) -- single-threaded test
  setenv("SIMPLISH_AGENT_PORT", "9123", 1);
  REQUIRE(editorAgentPortFromEnvironment() == 9123);

  // NOLINTNEXTLINE(concurrency-mt-unsafe) -- single-threaded test
  setenv("SIMPLISH_AGENT_PORT", "default", 1);
  REQUIRE(editorAgentPortFromEnvironment() == EDITOR_AGENT_DEFAULT_PORT);

  // NOLINTNEXTLINE(concurrency-mt-unsafe) -- single-threaded test
  unsetenv("SIMPLISH_AGENT_PORT");
}
