#include "enet-transport.h"

#include <algorithm>
#include <array>
#include <engine/net/net-lan-codec.h>
#include <engine/net/udp-lan-beacon.h>
#include <engine/net/udp-lan-game.h>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#endif

namespace eng::net {

namespace {

  /// Bytes a discovery datagram can hold: far more than an answer needs.
  constexpr std::size_t DATAGRAM_BYTES = 512;

  /// The platform's socket from how a beacon holds it.
  ENetSocket toSocket(intptr_t socket) {
    return static_cast<ENetSocket>(socket);
  }

  /// A non-blocking UDP socket that may broadcast and share its port;
  /// nothing when there is none to be had.
  std::optional<ENetSocket> datagramSocket() {
    const ENetSocket socket = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if (socket == ENET_SOCKET_NULL) {
      return std::nullopt;
    }
    (void)enet_socket_set_option(socket, ENET_SOCKOPT_NONBLOCK, 1);
    (void)enet_socket_set_option(socket, ENET_SOCKOPT_BROADCAST, 1);
    (void)enet_socket_set_option(socket, ENET_SOCKOPT_REUSEADDR, 1);
    return socket;
  }

  /// The next datagram waiting on @p socket, and who sent it; nothing
  /// when none is.
  std::optional<std::pair<ENetAddress, std::vector<std::byte>>>
  receive(ENetSocket socket) {
    std::array<std::byte, DATAGRAM_BYTES> data{};
    ENetBuffer buffer{data.data(), data.size()};
    ENetAddress from{};
    const int got = enet_socket_receive(socket, &from, &buffer, 1);
    if (got <= 0) {
      return std::nullopt;
    }
    const auto size = static_cast<std::size_t>(got);
    return std::pair{from, std::vector(data.begin(), data.begin() + size)};
  }

  /// Send @p bytes to @p to from @p socket.
  void sendTo(ENetSocket socket, const ENetAddress& to,
              const std::vector<std::byte>& bytes) {
    // ENet's buffer is not const-correct; sending only reads it.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    ENetBuffer buffer{const_cast<std::byte*>(bytes.data()), bytes.size()};
    (void)enet_socket_send(socket, &to, &buffer, 1);
  }

  /// @p address as dotted text.
  std::string hostText(const ENetAddress& address) {
    std::array<char, 64> text{};
    (void)enet_address_get_host_ip(&address, text.data(), text.size());
    return text.data();
  }

#ifndef _WIN32
  /// Keep @p interface's IPv4 broadcast address in @p targets, if it is up
  /// and has one.
  void keepBroadcast(const ifaddrs& interface,
                     std::vector<std::string>& targets) {
    const bool broadcasts = (interface.ifa_flags & IFF_UP) != 0 &&
                            (interface.ifa_flags & IFF_BROADCAST) != 0 &&
                            interface.ifa_broadaddr != nullptr &&
                            interface.ifa_broadaddr->sa_family == AF_INET;
    if (!broadcasts) {
      return;
    }
    std::array<char, INET_ADDRSTRLEN> text{};
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) sockaddr
    // family checked
    const auto* address =
        reinterpret_cast<const sockaddr_in*>(interface.ifa_broadaddr);
    if (inet_ntop(AF_INET, &address->sin_addr, text.data(), text.size()) !=
        nullptr) {
      targets.emplace_back(text.data());
    }
  }
#endif

  /// Whether @p games already has the server that sent @p game from
  /// @p host: the same session heard at another address, or the same port
  /// at the same address.
  bool known(const std::vector<UdpLanGame>& games, const std::string& host,
             const NetLanGame& game) {
    return std::ranges::any_of(games, [&](const UdpLanGame& found) {
      const bool same_session =
          game.session != 0 && found.game.session == game.session;
      return same_session ||
             (found.host == host && found.game.port == game.port);
    });
  }

  /// Keep the answer in @p datagram, if it is one and is new, in @p games.
  void
  keepAnswer(const std::pair<ENetAddress, std::vector<std::byte>>& datagram,
             std::vector<UdpLanGame>& games) {
    const std::optional<NetLanGame> game = decodeLanGame(datagram.second);
    const std::string host = hostText(datagram.first);
    if (game && !known(games, host, *game)) {
      games.push_back({host, *game});
    }
  }

  /// Where a query for the whole local network goes: each interface's own
  /// broadcast address — how the other machines on it hear it; macOS will
  /// not route the limited broadcast without a default route — then the
  /// limited broadcast where it is routed, then this machine itself, whose
  /// own broadcasts do not come back to it.
  std::vector<std::string> wholeNetworkTargets() {
    std::vector<std::string> targets;
#ifndef _WIN32
    ifaddrs* interfaces = nullptr;
    if (getifaddrs(&interfaces) == 0) {
      for (const ifaddrs* at = interfaces; at != nullptr; at = at->ifa_next) {
        keepBroadcast(*at, targets);
      }
      freeifaddrs(interfaces);
    }
#endif
    targets.emplace_back(UDP_LAN_BROADCAST);
    targets.emplace_back("127.0.0.1");
    return targets;
  }

  /// Send a query from @p socket to each of @p targets on @p port.
  void askAll(ENetSocket socket, const std::vector<std::string>& targets,
              uint16_t port) {
    for (const std::string& target : targets) {
      ENetAddress to{0, port};
      if (enet_address_set_host(&to, target.c_str()) == 0) {
        sendTo(socket, to, encodeLanQuery());
      }
    }
  }

  /// Keep every new answer that reaches @p socket in @p wait in @p games.
  void gather(ENetSocket socket, std::chrono::milliseconds wait,
              std::vector<UdpLanGame>& games) {
    const auto until = std::chrono::steady_clock::now() + wait;
    while (std::chrono::steady_clock::now() < until) {
      while (const auto datagram = receive(socket)) {
        keepAnswer(*datagram, games);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }

}  // namespace

std::unique_ptr<UdpLanBeacon> UdpLanBeacon::open(uint16_t port) {
  const std::optional<ENetSocket> socket =
      enetReady() ? datagramSocket() : std::nullopt;
  const ENetAddress address{ENET_HOST_ANY, port};
  if (!socket || enet_socket_bind(*socket, &address) != 0) {
    if (socket) {
      enet_socket_destroy(*socket);
    }
    return nullptr;
  }
  ENetAddress bound{};
  (void)enet_socket_get_address(*socket, &bound);
  return std::make_unique<UdpLanBeacon>(static_cast<intptr_t>(*socket),
                                        bound.port);
}

UdpLanBeacon::UdpLanBeacon(intptr_t socket, uint16_t port)
  : socket_(socket), port_(port) {}

UdpLanBeacon::~UdpLanBeacon() {
  enet_socket_destroy(toSocket(socket_));
}

void UdpLanBeacon::answer(const NetLanGame& game) const {
  while (const auto datagram = receive(toSocket(socket_))) {
    if (isLanQuery(datagram->second)) {
      sendTo(toSocket(socket_), datagram->first, encodeLanGame(game));
    }
  }
}

std::vector<UdpLanGame> findLanGames(const std::string& address, uint16_t port,
                                     std::chrono::milliseconds wait) {
  std::vector<UdpLanGame> games;
  const std::optional<ENetSocket> socket =
      enetReady() ? datagramSocket() : std::nullopt;
  if (!socket) {
    return games;
  }
  askAll(*socket,
         address == UDP_LAN_BROADCAST ? wholeNetworkTargets()
                                      : std::vector<std::string>{address},
         port);
  gather(*socket, wait, games);
  enet_socket_destroy(*socket);
  return games;
}

}  // namespace eng::net
