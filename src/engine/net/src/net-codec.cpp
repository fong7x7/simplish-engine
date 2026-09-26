#include <array>
#include <engine/net/net-codec.h>
#include <engine/sim/byte-reader.h>
#include <engine/sim/byte-writer.h>
#include <limits>
#include <string>
#include <utility>

namespace eng::net {

namespace {

  using sim::ByteReader;
  using sim::ByteWriter;

  /// A decoded message, or nothing when its bytes were not one.
  using Decoded = std::optional<NetMessage>;

  /// Largest zigzagged stick axis: -32768 is 65535.
  constexpr uint64_t MAX_ZIGZAG_AXIS = 65535;

  /// Signed to unsigned so small magnitudes of either sign stay small.
  uint64_t zigzag(int64_t value) {
    return (static_cast<uint64_t>(value) << 1U) ^
           static_cast<uint64_t>(value >> 63U);
  }

  int64_t unzigzag(uint64_t value) {
    return static_cast<int64_t>(value >> 1U) ^
           -static_cast<int64_t>(value & 1U);
  }

  /// A varint no greater than @p most.
  std::optional<uint64_t> bounded(ByteReader& in, uint64_t most) {
    const std::optional<uint64_t> value = in.varint();
    return value && *value <= most ? value : std::nullopt;
  }

  /// A byte no greater than @p most.
  std::optional<uint8_t> smallByte(ByteReader& in, uint64_t most) {
    const std::optional<uint8_t> value = in.u8();
    return value && *value <= most ? value : std::nullopt;
  }

  void writeString(ByteWriter& out, const std::string& text) {
    out.varint(text.size());
    out.bytes(std::as_bytes(std::span(text.data(), text.size())));
  }

  std::optional<std::string> readString(ByteReader& in) {
    const std::optional<uint64_t> size = bounded(in, NET_MAX_ID_BYTES);
    const auto bytes = size ? in.bytes(*size) : std::nullopt;
    if (!bytes) {
      return std::nullopt;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) bytes are
    // chars
    return std::string(reinterpret_cast<const char*>(bytes->data()),
                       bytes->size());
  }

  void writeInput(ByteWriter& out, const sim::PlayerInput& input) {
    for (const int16_t axis :
         {input.move_x, input.move_y, input.aim_x, input.aim_y}) {
      out.varint(zigzag(axis));
    }
    out.varint(input.buttons);
    out.varint(input.ui_action);
  }

  std::optional<int16_t> readAxis(ByteReader& in) {
    const std::optional<uint64_t> value = bounded(in, MAX_ZIGZAG_AXIS);
    return value ? std::optional{static_cast<int16_t>(unzigzag(*value))}
                 : std::nullopt;
  }

  /// A `PlayerInput`'s four stick axes, in field order.
  std::optional<std::array<int16_t, 4>> readAxes(ByteReader& in) {
    std::array<int16_t, 4> axis{};
    for (int16_t& one : axis) {
      const std::optional<int16_t> value = readAxis(in);
      if (!value) {
        return std::nullopt;
      }
      one = *value;
    }
    return axis;
  }

  std::optional<sim::PlayerInput> readInput(ByteReader& in) {
    constexpr uint64_t MAX_U32 = std::numeric_limits<uint32_t>::max();
    const std::optional<std::array<int16_t, 4>> axis = readAxes(in);
    const std::optional<uint64_t> buttons = bounded(in, MAX_U32);
    const std::optional<uint64_t> ui_action = bounded(in, MAX_U32);
    if (!axis || !buttons || !ui_action) {
      return std::nullopt;
    }
    sim::PlayerInput input{(*axis)[0], (*axis)[1], (*axis)[2],
                           (*axis)[3], 0,          0};
    input.buttons = static_cast<uint32_t>(*buttons);
    input.ui_action = static_cast<uint32_t>(*ui_action);
    return input;
  }

  void write(ByteWriter& out, const NetHello& hello) {
    out.u16(hello.protocol);
    out.u64(hello.content_hash);
    writeString(out, hello.character);
  }

  void write(ByteWriter& out, const NetWelcome& welcome) {
    out.u8(welcome.slot);
  }

  void write(ByteWriter& out, const NetRefusal& refusal) {
    out.u8(static_cast<uint8_t>(refusal.reason));
  }

  void write(ByteWriter& out, const NetRoster& roster) {
    out.u8(roster.seated);
  }

  void write(ByteWriter& out, const NetStart& start) {
    out.u16(start.run);
    writeString(out, start.header.level_id);
    out.u64(start.header.content_hash);
    out.u64(start.header.seed);
    out.u8(start.header.player_count);
    for (uint8_t slot = 0; slot < start.header.player_count; ++slot) {
      writeString(out, start.header.characters[slot]);
    }
    out.u8(start.input_delay);
  }

  void write(ByteWriter& out, const NetInput& input) {
    out.u16(input.run);
    out.varint(input.tick);
    writeInput(out, input.input);
  }

  void write(ByteWriter& out, const NetFrame& frame) {
    out.u16(frame.run);
    out.varint(frame.tick);
    out.u8(frame.absent);
    for (const sim::PlayerInput& player : frame.input.players) {
      writeInput(out, player);
    }
  }

  void write(ByteWriter& out, const NetHashReport& report) {
    out.u16(report.run);
    out.varint(report.hash.tick);
    out.u64(report.hash.combined);
    out.u8(static_cast<uint8_t>(report.hash.section_count));
    for (const sim::TickHashSection& section : report.hash.activeSections()) {
      out.u64(section.hash);
    }
  }

  void write(ByteWriter& out, const NetDesync& desync) {
    out.u16(desync.run);
    out.varint(desync.tick);
    out.u8(desync.section);
    out.u8(desync.slot);
  }

  void write(ByteWriter& out, const NetEnd& end) {
    out.u16(end.run);
  }

  Decoded readHello(ByteReader& in) {
    const std::optional<uint16_t> protocol = in.u16();
    const std::optional<uint64_t> content = in.u64();
    std::optional<std::string> character = readString(in);
    if (!protocol || !content || !character) {
      return std::nullopt;
    }
    return NetHello{*protocol, *content, std::move(*character)};
  }

  Decoded readWelcome(ByteReader& in) {
    const std::optional<uint8_t> slot = smallByte(in, sim::MAX_PLAYERS - 1);
    return slot ? Decoded{NetWelcome{*slot}} : std::nullopt;
  }

  Decoded readRefusal(ByteReader& in) {
    const std::optional<uint8_t> reason =
        smallByte(in, static_cast<uint8_t>(NetRefusalReason::FULL));
    return reason ? Decoded{NetRefusal{static_cast<NetRefusalReason>(*reason)}}
                  : std::nullopt;
  }

  Decoded readRoster(ByteReader& in) {
    const std::optional<uint8_t> seated = in.u8();
    return seated ? Decoded{NetRoster{*seated}} : std::nullopt;
  }

  /// Every seat's character, into @p header, for its player count.
  bool readCharacters(ByteReader& in, sim::ReplayHeader& header) {
    for (uint8_t slot = 0; slot < header.player_count; ++slot) {
      std::optional<std::string> character = readString(in);
      if (!character) {
        return false;
      }
      header.characters[slot] = std::move(*character);
    }
    return true;
  }

  /// What a run starts from, as a `NetStart` carries it.
  std::optional<sim::ReplayHeader> readHeader(ByteReader& in) {
    std::optional<std::string> level = readString(in);
    const std::optional<uint64_t> content = in.u64();
    const std::optional<uint64_t> seed = in.u64();
    const std::optional<uint8_t> players = smallByte(in, sim::MAX_PLAYERS);
    if (!level || !content || !seed || !players || *players == 0) {
      return std::nullopt;
    }
    sim::ReplayHeader header{std::move(*level), *content, *seed, *players, {}};
    return readCharacters(in, header) ? std::optional{std::move(header)}
                                      : std::nullopt;
  }

  Decoded readStart(ByteReader& in) {
    const std::optional<uint16_t> run = in.u16();
    std::optional<sim::ReplayHeader> header =
        run ? readHeader(in) : std::nullopt;
    const std::optional<uint8_t> delay = header ? in.u8() : std::nullopt;
    if (!delay) {
      return std::nullopt;
    }
    return NetStart{*run, std::move(*header), *delay};
  }

  Decoded readNetInput(ByteReader& in) {
    const std::optional<uint16_t> run = in.u16();
    const std::optional<uint64_t> tick = in.varint();
    const std::optional<sim::PlayerInput> input = readInput(in);
    if (!run || !tick || !input) {
      return std::nullopt;
    }
    return NetInput{*run, *tick, *input};
  }

  Decoded readFrame(ByteReader& in) {
    const std::optional<uint16_t> run = in.u16();
    const std::optional<uint64_t> tick = in.varint();
    const std::optional<uint8_t> absent = in.u8();
    if (!run || !tick || !absent) {
      return std::nullopt;
    }
    NetFrame frame{*run, *tick, *absent, {}};
    for (sim::PlayerInput& player : frame.input.players) {
      const std::optional<sim::PlayerInput> input = readInput(in);
      if (!input) {
        return std::nullopt;
      }
      player = *input;
    }
    return frame;
  }

  /// @p count section hashes into @p hash.
  bool readSections(ByteReader& in, uint8_t count, sim::TickHash& hash) {
    hash.section_count = count;
    for (uint8_t i = 0; i < count; ++i) {
      const std::optional<uint64_t> section = in.u64();
      if (!section) {
        return false;
      }
      hash.sections[i].hash = *section;
    }
    return true;
  }

  Decoded readHashReport(ByteReader& in) {
    const std::optional<uint16_t> run = in.u16();
    const std::optional<uint64_t> tick = in.varint();
    const std::optional<uint64_t> combined = in.u64();
    const std::optional<uint8_t> count =
        smallByte(in, sim::MAX_TICK_HASH_SECTIONS);
    NetHashReport report{run.value_or(0), {}};
    report.hash.tick = tick.value_or(0);
    report.hash.combined = combined.value_or(0);
    if (!run || !tick || !combined || !count ||
        !readSections(in, *count, report.hash)) {
      return std::nullopt;
    }
    return report;
  }

  Decoded readDesync(ByteReader& in) {
    const std::optional<uint16_t> run = in.u16();
    const std::optional<uint64_t> tick = in.varint();
    const std::optional<uint8_t> section = in.u8();
    const std::optional<uint8_t> slot = in.u8();
    if (!run || !tick || !section || !slot) {
      return std::nullopt;
    }
    return NetDesync{*run, *tick, *section, *slot};
  }

  Decoded readEnd(ByteReader& in) {
    const std::optional<uint16_t> run = in.u16();
    return run ? Decoded{NetEnd{*run}} : std::nullopt;
  }

  /// A message's decoder.
  using Decoder = Decoded (*)(ByteReader&);

  /// The decoder of each kind, in `NetMessage`'s order.
  constexpr std::array<Decoder, std::variant_size_v<NetMessage>> DECODERS = {
      readHello,    readWelcome, readRefusal,    readRoster, readStart,
      readNetInput, readFrame,   readHashReport, readDesync, readEnd,
  };

}  // namespace

std::vector<std::byte> encodeNetMessage(const NetMessage& message) {
  ByteWriter out;
  out.u8(static_cast<uint8_t>(message.index()));
  std::visit([&out](const auto& body) { write(out, body); }, message);
  return out.take();
}

std::optional<NetMessage> decodeNetMessage(std::span<const std::byte> bytes) {
  ByteReader in(bytes);
  const std::optional<uint8_t> kind = in.u8();
  if (!kind || *kind >= DECODERS.size()) {
    return std::nullopt;
  }
  Decoded message = DECODERS.at(*kind)(in);
  return message && in.atEnd() ? std::move(message) : std::nullopt;
}

}  // namespace eng::net
