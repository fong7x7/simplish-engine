#include "byte-reader.h"
#include "byte-writer.h"

#include <algorithm>
#include <array>
#include <engine/sim/replay-codec.h>
#include <string>
#include <utility>

namespace eng::sim {

namespace {

  /// Every replay file starts with these four bytes.
  constexpr std::array<std::byte, 4> MAGIC = {std::byte{'S'}, std::byte{'R'},
                                              std::byte{'P'}, std::byte{'L'}};

  /// Longest level id a replay may carry.
  constexpr uint64_t MAX_LEVEL_ID_BYTES = 256;

  /// Axes in a `PlayerInput`: move X and Y, aim X and Y.
  constexpr std::size_t AXIS_COUNT = 4;

  /// Change-mask bit for the buttons, after one bit per axis.
  constexpr uint8_t BUTTONS_BIT = 1U << AXIS_COUNT;

  /// Every bit a change mask may set.
  constexpr uint8_t KNOWN_FIELDS = (BUTTONS_BIT << 1U) - 1U;

  /// Largest zigzag-encoded axis delta: from one end of int16 to the other.
  constexpr uint64_t MAX_AXIS_DELTA = 2U * 65535U;

  /// A decoding step's failure, or nothing when it succeeded.
  using Failure = std::optional<ReplayDecodeError>;

  std::array<int16_t, AXIS_COUNT> axes(const PlayerInput& input) {
    return {input.move_x, input.move_y, input.aim_x, input.aim_y};
  }

  void setAxes(PlayerInput& input,
               const std::array<int16_t, AXIS_COUNT>& axis) {
    input.move_x = axis[0];
    input.move_y = axis[1];
    input.aim_x = axis[2];
    input.aim_y = axis[3];
  }

  /// Signed to unsigned so small magnitudes of either sign stay small.
  uint64_t zigzag(int64_t value) {
    return (static_cast<uint64_t>(value) << 1U) ^
           static_cast<uint64_t>(value >> 63U);
  }

  int64_t unzigzag(uint64_t value) {
    return static_cast<int64_t>(value >> 1U) ^
           -static_cast<int64_t>(value & 1U);
  }

  // ---------------------------------------------------------------------------
  // Encoding
  // ---------------------------------------------------------------------------

  uint8_t changeMask(const PlayerInput& from, const PlayerInput& to) {
    const auto old_axes = axes(from);
    const auto new_axes = axes(to);
    unsigned mask = 0;
    for (std::size_t i = 0; i < AXIS_COUNT; ++i) {
      mask |= old_axes[i] != new_axes[i] ? 1U << i : 0U;
    }
    mask |= from.buttons != to.buttons ? BUTTONS_BIT : 0U;
    return static_cast<uint8_t>(mask);
  }

  void encodePlayer(ByteWriter& out, const PlayerInput& from,
                    const PlayerInput& to) {
    const uint8_t mask = changeMask(from, to);
    out.u8(mask);
    const auto old_axes = axes(from);
    const auto new_axes = axes(to);
    for (std::size_t i = 0; i < AXIS_COUNT; ++i) {
      if ((mask & (1U << i)) != 0U) {
        out.varint(zigzag(int64_t{new_axes[i]} - int64_t{old_axes[i]}));
      }
    }
    if ((mask & BUTTONS_BIT) != 0U) {
      out.varint(from.buttons ^ to.buttons);
    }
  }

  bool samePlayers(const TickInput& a, const TickInput& b, uint8_t players) {
    return std::equal(a.players.begin(), a.players.begin() + players,
                      b.players.begin());
  }

  void encodeHeader(ByteWriter& out, const ReplayHeader& header) {
    out.bytes(MAGIC);
    out.u16(REPLAY_FORMAT_VERSION);
    out.u8(header.player_count);
    out.u64(header.seed);
    out.u64(header.content_hash);
    out.varint(header.level_id.size());
    out.bytes(std::as_bytes(
        std::span<const char>(header.level_id.data(), header.level_id.size())));
  }

  void encodeInputs(ByteWriter& out, const Replay& replay) {
    const uint8_t players = replay.header.player_count;
    out.varint(replay.inputs.size());
    TickInput previous{};
    uint64_t unchanged = 0;
    for (const TickInput& input : replay.inputs) {
      if (samePlayers(previous, input, players)) {
        ++unchanged;
        continue;
      }
      out.varint(std::exchange(unchanged, 0U));
      for (uint8_t player = 0; player < players; ++player) {
        encodePlayer(out, previous.players[player], input.players[player]);
      }
      previous = input;
    }
    out.varint(unchanged);
  }

  void encodeCheckpoints(ByteWriter& out, const std::vector<TickHash>& hashes) {
    out.varint(hashes.size());
    for (const TickHash& hash : hashes) {
      out.varint(hash.tick);
      out.u64(hash.combined);
      out.u8(static_cast<uint8_t>(hash.section_count));
      for (const TickHashSection& section : hash.activeSections()) {
        out.u64(section.hash);
      }
    }
  }

  // ---------------------------------------------------------------------------
  // Decoding
  // ---------------------------------------------------------------------------

  /// Why a read from `in` failed: it ran out of bytes, or it found bytes that
  /// cannot be what was being read.
  ReplayDecodeError readFailure(const ByteReader& in) {
    return in.overran() ? ReplayDecodeError::TRUNCATED
                        : ReplayDecodeError::MALFORMED;
  }

  /// A varint no greater than `max`; a larger one is malformed.
  std::expected<uint64_t, ReplayDecodeError> readVarint(ByteReader& in,
                                                        uint64_t max) {
    const auto value = in.varint();
    if (!value) {
      return std::unexpected<ReplayDecodeError>(readFailure(in));
    }
    if (*value > max) {
      return std::unexpected<ReplayDecodeError>(ReplayDecodeError::MALFORMED);
    }
    return *value;
  }

  Failure decodeLevelId(ByteReader& in, std::string& level_id) {
    const auto size = readVarint(in, MAX_LEVEL_ID_BYTES);
    if (!size) {
      return size.error();
    }
    const auto id = in.bytes(*size);
    if (!id) {
      return readFailure(in);
    }
    level_id.clear();
    for (const std::byte byte : *id) {
      level_id.push_back(static_cast<char>(byte));
    }
    return std::nullopt;
  }

  Failure decodeSession(ByteReader& in, ReplayHeader& header) {
    const auto players = in.u8();
    const auto seed = in.u64();
    const auto content_hash = in.u64();
    if (!players || !seed || !content_hash) {
      return readFailure(in);
    }
    if (*players < 1 || *players > MAX_PLAYERS) {
      return ReplayDecodeError::MALFORMED;
    }
    header.player_count = *players;
    header.seed = *seed;
    header.content_hash = *content_hash;
    return decodeLevelId(in, header.level_id);
  }

  Failure decodeHeader(ByteReader& in, ReplayHeader& header) {
    const auto magic = in.bytes(MAGIC.size());
    if (!magic) {
      return readFailure(in);
    }
    if (!std::equal(magic->begin(), magic->end(), MAGIC.begin())) {
      return ReplayDecodeError::BAD_MAGIC;
    }
    const auto version = in.u16();
    if (!version) {
      return readFailure(in);
    }
    if (*version != REPLAY_FORMAT_VERSION) {
      return ReplayDecodeError::UNSUPPORTED_VERSION;
    }
    return decodeSession(in, header);
  }

  Failure decodeAxis(ByteReader& in, int16_t& axis) {
    const auto delta = readVarint(in, MAX_AXIS_DELTA);
    if (!delta) {
      return delta.error();
    }
    const int64_t value = int64_t{axis} + unzigzag(*delta);
    if (value < INT16_MIN || value > INT16_MAX) {
      return ReplayDecodeError::MALFORMED;
    }
    axis = static_cast<int16_t>(value);
    return std::nullopt;
  }

  Failure decodeButtons(ByteReader& in, uint8_t mask, uint32_t& buttons) {
    if ((mask & BUTTONS_BIT) == 0U) {
      return std::nullopt;
    }
    const auto flipped = readVarint(in, UINT32_MAX);
    if (!flipped) {
      return flipped.error();
    }
    buttons ^= static_cast<uint32_t>(*flipped);
    return std::nullopt;
  }

  /// Applies the axis deltas `mask` flags to `input`.
  Failure decodeAxes(ByteReader& in, uint8_t mask, PlayerInput& input) {
    auto values = axes(input);
    for (std::size_t i = 0; i < AXIS_COUNT; ++i) {
      if ((mask & (1U << i)) == 0U) {
        continue;
      }
      if (const auto failure = decodeAxis(in, values[i])) {
        return failure;
      }
    }
    setAxes(input, values);
    return std::nullopt;
  }

  Failure decodePlayer(ByteReader& in, PlayerInput& input) {
    const auto mask = in.u8();
    if (!mask) {
      return readFailure(in);
    }
    if ((*mask & ~KNOWN_FIELDS) != 0U) {
      return ReplayDecodeError::MALFORMED;
    }
    if (const auto failure = decodeAxes(in, *mask, input)) {
      return failure;
    }
    return decodeButtons(in, *mask, input.buttons);
  }

  Failure decodeTick(ByteReader& in, uint8_t players, TickInput& input) {
    for (uint8_t player = 0; player < players; ++player) {
      if (const auto failure = decodePlayer(in, input.players[player])) {
        return failure;
      }
    }
    return std::nullopt;
  }

  /// Reads alternating runs of unchanged ticks and changed ticks until there
  /// are `count` inputs. Each changed tick is a delta from the input before
  /// it, or from zero for the first.
  Failure decodeRuns(ByteReader& in, uint8_t players, uint64_t count,
                     std::vector<TickInput>& inputs) {
    while (true) {
      const TickInput previous = inputs.empty() ? TickInput{} : inputs.back();
      const auto unchanged = readVarint(in, count - inputs.size());
      if (!unchanged) {
        return unchanged.error();
      }
      inputs.insert(inputs.end(), *unchanged, previous);
      if (inputs.size() == count) {
        return std::nullopt;
      }
      inputs.push_back(previous);
      if (const auto failure = decodeTick(in, players, inputs.back())) {
        return failure;
      }
    }
  }

  Failure decodeInputs(ByteReader& in, uint8_t players,
                       std::vector<TickInput>& inputs) {
    const auto count = readVarint(in, MAX_REPLAY_TICKS);
    if (!count) {
      return count.error();
    }
    return decodeRuns(in, players, *count, inputs);
  }

  Failure decodeSectionHashes(ByteReader& in, TickHash& hash) {
    for (TickHashSection& section :
         std::span(hash.sections).first(hash.section_count)) {
      const auto value = in.u64();
      if (!value) {
        return readFailure(in);
      }
      section.hash = *value;
    }
    return std::nullopt;
  }

  Failure decodeCheckpoint(ByteReader& in, TickHash& hash) {
    const auto tick = in.varint();
    const auto combined = in.u64();
    const auto sections = in.u8();
    if (!tick || !combined || !sections) {
      return readFailure(in);
    }
    if (*sections > MAX_TICK_HASH_SECTIONS) {
      return ReplayDecodeError::MALFORMED;
    }
    hash = TickHash{*tick, *combined, {}, *sections};
    return decodeSectionHashes(in, hash);
  }

  /// Checkpoints must name ticks the replay has, in strictly ascending order.
  Failure decodeCheckpoints(ByteReader& in, uint64_t tick_count,
                            std::vector<TickHash>& hashes) {
    const auto count = readVarint(in, tick_count);
    if (!count) {
      return count.error();
    }
    for (uint64_t i = 0; i < *count; ++i) {
      TickHash hash;
      if (const auto failure = decodeCheckpoint(in, hash)) {
        return failure;
      }
      const uint64_t earliest = hashes.empty() ? 0 : hashes.back().tick + 1U;
      if (hash.tick < earliest || hash.tick >= tick_count) {
        return ReplayDecodeError::MALFORMED;
      }
      hashes.push_back(hash);
    }
    return std::nullopt;
  }

}  // namespace

std::vector<std::byte> encodeReplay(const Replay& replay) {
  ByteWriter out;
  encodeHeader(out, replay.header);
  encodeInputs(out, replay);
  encodeCheckpoints(out, replay.checkpoints);
  return out.take();
}

std::expected<Replay, ReplayDecodeError>
decodeReplay(std::span<const std::byte> bytes) {
  ByteReader in(bytes);
  Replay replay;
  Failure failure = decodeHeader(in, replay.header);
  if (!failure) {
    failure = decodeInputs(in, replay.header.player_count, replay.inputs);
  }
  if (!failure) {
    failure = decodeCheckpoints(in, replay.inputs.size(), replay.checkpoints);
  }
  if (!failure && !in.atEnd()) {
    failure = ReplayDecodeError::MALFORMED;
  }
  if (failure) {
    return std::unexpected<ReplayDecodeError>(*failure);
  }
  return replay;
}

}  // namespace eng::sim
