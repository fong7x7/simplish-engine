#include <catch2/catch_test_macros.hpp>
#include <engine/net/udp-lan-beacon.h>
#include <engine/net/udp-lan-game.h>
#include <future>

using namespace eng::net;

namespace {

/// Answer queries on @p beacon, saying @p game, until @p done is set.
void answerUntil(UdpLanBeacon& beacon, const NetLanGame& game,
                 const std::future<void>& done) {
  while (done.wait_for(std::chrono::milliseconds(2)) !=
         std::future_status::ready) {
    beacon.answer(game);
  }
}

}  // namespace

TEST_CASE("a LAN beacon answers a query sent to it with what it says") {
  const auto beacon = UdpLanBeacon::open(0);
  REQUIRE(beacon);
  const NetLanGame game{3, 1, 2, 47015, 1, 4, 0, 1, "Arena", 42};
  std::promise<void> stop;
  std::thread answering(answerUntil, std::ref(*beacon), game,
                        stop.get_future());
  const auto found =
      findLanGames("127.0.0.1", beacon->port(), std::chrono::milliseconds(300));
  stop.set_value();
  answering.join();
  REQUIRE(found.size() == 1);
  CHECK(found[0].host == "127.0.0.1");
  CHECK(found[0].game == game);
}

TEST_CASE("asking where nobody answers finds nothing") {
  const auto beacon = UdpLanBeacon::open(0);
  REQUIRE(beacon);
  const uint16_t silent = beacon->port();
  CHECK(findLanGames("127.0.0.1", silent, std::chrono::milliseconds(100))
            .empty());
  CHECK(findLanGames("not a host", silent, std::chrono::milliseconds(10))
            .empty());
}
