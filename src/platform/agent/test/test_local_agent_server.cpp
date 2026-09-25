#include <catch2/catch_test_macros.hpp>
#include <engine/agent/local-agent-server.h>
#include <string>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace eng::agent;

namespace {

/// A blocking client connection to the loopback @p port, sent @p request.
int connectAndSend(uint16_t port, const std::string& request) {
  const int client = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  REQUIRE(::connect(client, reinterpret_cast<const sockaddr*>(&address),
                    sizeof(address)) == 0);
  REQUIRE(::send(client, request.data(), request.size(), 0) ==
          static_cast<ssize_t>(request.size()));
  return client;
}

/// Everything @p client receives until the server closes it.
std::string receiveAll(int client) {
  std::string got;
  char chunk[1024];
  for (ssize_t n = ::recv(client, chunk, sizeof(chunk), 0); n > 0;
       n = ::recv(client, chunk, sizeof(chunk), 0)) {
    got.append(chunk, static_cast<size_t>(n));
  }
  return got;
}

}  // namespace

TEST_CASE("a handler that answers later is asked again until it answers") {
  LocalAgentServer server;
  REQUIRE(server.open(0));
  const int client =
      connectAndSend(server.port(), "GET /state HTTP/1.1\r\nHost: x\r\n\r\n");
  int asked = 0;
  const auto handler = [&asked](const AgentHttpRequest&) {
    ++asked;
    return AgentHttpResponse{200, R"({"n":)" + std::to_string(asked) + "}",
                             asked < 3 ? AgentReplyTiming::LATER
                                       : AgentReplyTiming::NOW};
  };

  for (int poll = 0; poll < 50 && asked < 3; ++poll) {
    server.poll(handler);
  }

  CHECK(asked == 3);
  CHECK(receiveAll(client).find(R"({"n":3})") != std::string::npos);
  ::close(client);
}
#endif
