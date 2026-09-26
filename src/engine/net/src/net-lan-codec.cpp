#include <algorithm>
#include <array>
#include <engine/net/net-lan-codec.h>
#include <engine/sim/byte-reader.h>
#include <engine/sim/byte-writer.h>
#include <string>

namespace eng::net {

namespace {

  /// Every discovery datagram starts with these four bytes.
  constexpr std::array<std::byte, 4> MAGIC = {std::byte{'S'}, std::byte{'M'},
                                              std::byte{'P'}, std::byte{'L'}};

  /// The kind byte of a query.
  constexpr std::byte QUERY{'Q'};

  /// The kind byte of an answer.
  constexpr std::byte GAME{'G'};

  /// A writer holding the magic and @p kind.
  sim::ByteWriter framed(std::byte kind) {
    sim::ByteWriter out;
    out.bytes(MAGIC);
    out.bytes(std::span(&kind, 1));
    return out;
  }

  /// Whether @p in starts with the magic and @p kind, reading past them.
  bool readFrame(sim::ByteReader& in, std::byte kind) {
    const auto head = in.bytes(MAGIC.size() + 1);
    return head && std::equal(MAGIC.begin(), MAGIC.end(), head->begin()) &&
           (*head)[MAGIC.size()] == kind;
  }

  /// A name: a byte of length, at most `NET_MAX_LAN_NAME_BYTES`, then it.
  std::optional<std::string> readName(sim::ByteReader& in) {
    const std::optional<uint8_t> size = in.u8();
    const auto bytes = size && *size <= NET_MAX_LAN_NAME_BYTES ? in.bytes(*size)
                                                               : std::nullopt;
    if (!bytes) {
      return std::nullopt;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) bytes are
    // chars
    return std::string(reinterpret_cast<const char*>(bytes->data()),
                       bytes->size());
  }

  /// The identifying fields of an answer, into @p game.
  bool readHead(sim::ByteReader& in, NetLanGame& game) {
    const auto protocol = in.u16();
    const auto build = in.u64();
    const auto content = in.u64();
    const auto port = in.u16();
    game.protocol = protocol.value_or(0);
    game.build = build.value_or(0);
    game.content_hash = content.value_or(0);
    game.port = port.value_or(0);
    return protocol && build && content && port;
  }

  /// The four one-byte fields of an answer, into @p game.
  bool readCounts(sim::ByteReader& in, NetLanGame& game) {
    for (uint8_t* field :
         {&game.seated, &game.seats, &game.running, &game.locked}) {
      const std::optional<uint8_t> value = in.u8();
      if (!value) {
        return false;
      }
      *field = *value;
    }
    return true;
  }

}  // namespace

std::vector<std::byte> encodeLanQuery() {
  return framed(QUERY).take();
}

bool isLanQuery(std::span<const std::byte> bytes) {
  sim::ByteReader in(bytes);
  return readFrame(in, QUERY) && in.atEnd();
}

std::vector<std::byte> encodeLanGame(const NetLanGame& game) {
  sim::ByteWriter out = framed(GAME);
  out.u16(game.protocol);
  out.u64(game.build);
  out.u64(game.content_hash);
  out.u16(game.port);
  for (const uint8_t count :
       {game.seated, game.seats, game.running, game.locked}) {
    out.u8(count);
  }
  const std::string name = game.name.substr(0, NET_MAX_LAN_NAME_BYTES);
  out.u8(static_cast<uint8_t>(name.size()));
  out.bytes(std::as_bytes(std::span(name.data(), name.size())));
  out.u64(game.session);
  return out.take();
}

std::optional<NetLanGame> decodeLanGame(std::span<const std::byte> bytes) {
  sim::ByteReader in(bytes);
  NetLanGame game;
  if (!readFrame(in, GAME) || !readHead(in, game) || !readCounts(in, game)) {
    return std::nullopt;
  }
  std::optional<std::string> name = readName(in);
  const std::optional<uint64_t> session = name ? in.u64() : std::nullopt;
  if (!session || !in.atEnd()) {
    return std::nullopt;
  }
  game.name = std::move(*name);
  game.session = *session;
  return game;
}

}  // namespace eng::net
