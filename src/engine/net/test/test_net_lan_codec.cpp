#include <catch2/catch_test_macros.hpp>
#include <engine/net/net-lan-codec.h>
#include <string>

using namespace eng::net;

namespace {

NetLanGame sample() {
  return {3, 0xB111D, 0xC0FFEE, 47015, 2, 4, 1, 1, "Friday night", 0x5E55};
}

}  // namespace

TEST_CASE("a LAN query is recognised, and nothing else is") {
  CHECK(isLanQuery(encodeLanQuery()));
  CHECK_FALSE(isLanQuery(encodeLanGame(sample())));
  auto longer = encodeLanQuery();
  longer.push_back(std::byte{0});
  CHECK_FALSE(isLanQuery(longer));
}

TEST_CASE("a server's LAN answer decodes to what it said") {
  CHECK(decodeLanGame(encodeLanGame(sample())) == sample());
}

TEST_CASE("a LAN answer's name is cut to the longest it may be") {
  NetLanGame game = sample();
  game.name = std::string(NET_MAX_LAN_NAME_BYTES + 10, 'x');
  CHECK(decodeLanGame(encodeLanGame(game))->name.size() ==
        NET_MAX_LAN_NAME_BYTES);
}

TEST_CASE("a truncated, padded or corrupted LAN answer decodes safely") {
  const auto bytes = encodeLanGame(sample());
  for (std::size_t size = 0; size < bytes.size(); ++size) {
    CHECK_FALSE(decodeLanGame(std::span(bytes.data(), size)));
  }
  auto padded = bytes;
  padded.push_back(std::byte{0});
  CHECK_FALSE(decodeLanGame(padded));
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    auto corrupt = bytes;
    corrupt[i] = std::byte{0xFF};
    (void)decodeLanGame(corrupt);  // Must not read out of bounds.
  }
  CHECK_FALSE(decodeLanGame(encodeLanQuery()));
}
