#include "lan-text.h"

#include <catch2/catch_test_macros.hpp>
#include <engine/net/net-hello.h>
#include <vector>

using namespace eng;
using namespace eng::editor;

namespace {

/// A session at @p host with @p seated of four seats, built as build 7
/// with content 9 unless told otherwise.
net::UdpLanGame session(const std::string& host, uint8_t seated,
                        uint8_t running = 0, uint64_t content = 9) {
  return {host,
          {net::NET_PROTOCOL_VERSION, 7, content, 47015, seated, 4, running, 0,
           "Arena"}};
}

}  // namespace

TEST_CASE("a LAN session reads as where, what and who, and what keeps you "
          "out") {
  CHECK(lanGameText(session("10.0.0.2", 1), 7, 9) ==
        "10.0.0.2:47015  Arena  1/4 players  waiting for players");
  CHECK(lanGameText(session("10.0.0.2", 4, 1), 7, 9) ==
        "10.0.0.2:47015  Arena  4/4 players  playing  (full)");
  CHECK(lanGameText(session("10.0.0.2", 1), 7, 8) ==
        "10.0.0.2:47015  Arena  1/4 players  waiting for players  (other "
        "content)");
  CHECK(lanGameText(session("10.0.0.2", 1), 6, 9)
            .ends_with("(another version of the game)"));
}

TEST_CASE("joining the LAN picks the first session that would take you "
          "now") {
  const std::vector<net::UdpLanGame> games = {
      session("10.0.0.1", 1, 0, 8),  // other content
      session("10.0.0.2", 4),        // full
      session("10.0.0.3", 2, 1),     // mid-run
      session("10.0.0.4", 1), session("10.0.0.5", 0)};
  const auto picked = pickLanGame(games, 7, 9);
  REQUIRE(picked);
  CHECK(picked->host == "10.0.0.4");
  CHECK_FALSE(pickLanGame(std::span(games).first(3), 7, 9));
}
