#include <catch2/catch_test_macros.hpp>
#include <engine/agent/agent-http-parse.h>
#include <string>

using namespace eng;
using namespace eng::agent;

TEST_CASE("a request is not complete until its blank line has arrived") {
  REQUIRE_FALSE(agentHttpRequestLength("GET /tools HTTP/1.1\r\n").has_value());
  REQUIRE(agentHttpRequestLength("GET /tools HTTP/1.1\r\n\r\n").has_value());
}

TEST_CASE("a request with a body waits for all of the body") {
  const std::string head = "POST /call HTTP/1.1\r\nContent-Length: 5\r\n\r\n";

  REQUIRE_FALSE(agentHttpRequestLength(head + "abc").has_value());
  REQUIRE(agentHttpRequestLength(head + "abcde") == head.size() + 5);
}

TEST_CASE("the content length header is read whatever case it is sent in") {
  const std::string head = "POST /call HTTP/1.1\r\ncontent-length:  4\r\n\r\n";

  REQUIRE(agentHttpRequestLength(head + "abcd") == head.size() + 4);
}

TEST_CASE("a second request in the buffer is not counted as part of this one") {
  const std::string first = "GET /a HTTP/1.1\r\n\r\n";

  REQUIRE(agentHttpRequestLength(first + "GET /b HTTP/1.1\r\n\r\n") ==
          first.size());
}

TEST_CASE("parsing takes the method, the path, and the body") {
  const AgentHttpRequest request = parseAgentHttpRequest(
      "POST /tools/translate HTTP/1.1\r\nContent-Length: 2\r\n\r\n{}");

  REQUIRE(request.method == "POST");
  REQUIRE(request.path == "/tools/translate");
  REQUIRE(request.body == "{}");
}

TEST_CASE("a query string is not part of the path anything routes on") {
  const AgentHttpRequest request =
      parseAgentHttpRequest("GET /state?pretty=1 HTTP/1.1\r\n\r\n");

  REQUIRE(request.path == "/state");
  REQUIRE(request.body.empty());
}

TEST_CASE("a response declares its length and closes the connection") {
  const std::string text = formatAgentHttpResponse(200, "{\"a\":1}");

  REQUIRE(text.starts_with("HTTP/1.1 200 OK\r\n"));
  REQUIRE(text.find("Content-Length: 7\r\n") != std::string::npos);
  REQUIRE(text.find("Content-Type: application/json\r\n") != std::string::npos);
  REQUIRE(text.find("Connection: close\r\n") != std::string::npos);
  REQUIRE(text.ends_with("\r\n\r\n{\"a\":1}"));
}
