#include "lan-text.h"

#include <engine/net/net-hello.h>
#include <string_view>

namespace eng::editor {

namespace {

  /// What stands in the way of joining @p found, or empty.
  std::string_view obstacle(const net::UdpLanGame& found, uint64_t build,
                            uint64_t content) {
    const net::NetLanGame& game = found.game;
    if (game.protocol != net::NET_PROTOCOL_VERSION || game.build != build) {
      return "  (another version of the game)";
    }
    if (game.content_hash != content) {
      return "  (other content)";
    }
    return game.seated >= game.seats ? "  (full)" : "";
  }

}  // namespace

bool lanGameCompatible(const net::UdpLanGame& found, uint64_t build,
                       uint64_t content) {
  return found.game.protocol == net::NET_PROTOCOL_VERSION &&
         found.game.build == build && found.game.content_hash == content;
}

std::string lanGameText(const net::UdpLanGame& found, uint64_t build,
                        uint64_t content) {
  const net::NetLanGame& game = found.game;
  return found.host + ":" + std::to_string(game.port) + "  " + game.name +
         "  " + std::to_string(game.seated) + "/" + std::to_string(game.seats) +
         " players  " +
         (game.running != 0 ? "playing" : "waiting for players") +
         (game.locked != 0 ? "  password" : "") +
         std::string(obstacle(found, build, content));
}

std::optional<net::UdpLanGame>
pickLanGame(std::span<const net::UdpLanGame> games, uint64_t build,
            uint64_t content) {
  for (const net::UdpLanGame& found : games) {
    if (lanGameCompatible(found, build, content) && found.game.running == 0 &&
        found.game.seated < found.game.seats) {
      return found;
    }
  }
  return std::nullopt;
}

}  // namespace eng::editor
