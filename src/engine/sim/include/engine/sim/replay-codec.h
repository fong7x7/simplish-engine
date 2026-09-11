#pragma once

/// @file replay-codec.h
/// @brief A replay as bytes, and back.
/// @par Threading
/// Pure functions.

#include <cstddef>
#include <cstdint>
#include <engine/core/expected-polyfill.h>
#include <engine/sim/replay.h>
#include <span>
#include <vector>

namespace eng::sim {

/// The replay format version `encodeReplay` writes and `decodeReplay` reads.
inline constexpr uint16_t REPLAY_FORMAT_VERSION = 2;

/// Longest replay `decodeReplay` accepts: six hours at 60 Hz. A bound, so a
/// corrupt tick count cannot ask the decoder for gigabytes.
inline constexpr uint64_t MAX_REPLAY_TICKS = 60ULL * 60ULL * 60ULL * 6ULL;

/// Why a buffer is not a replay.
enum class ReplayDecodeError : uint8_t {
  TRUNCATED,            ///< The buffer ends partway through the replay
  BAD_MAGIC,            ///< The buffer does not start with a replay's magic
  UNSUPPORTED_VERSION,  ///< A format version this build cannot read
  MALFORMED,            ///< Every byte is present, but they are not a replay
};

/// Encodes `replay` in the replay format.
///
/// Header: the magic `SRPL`, the format version (u16), the player count
/// (u8), the seed and content hash (u64 each), the level id as a varint
/// length and its bytes, then each player's character the same way, one
/// per player in the session. Fixed-width integers are little-endian;
/// varints are LEB128. Version 1 had no characters, and is not read.
///
/// Inputs: a varint tick count, then alternating runs — a varint count of
/// ticks identical to the one before, then one changed tick. A changed tick
/// writes, per player, a byte flagging which of its five fields changed and
/// then only those: axis deltas as zigzag varints, buttons as the XOR of
/// old and new. A final run closes the stream. Held input costs nothing; a
/// moving stick costs a few bytes a tick, not twelve per player.
///
/// Checkpoints: a varint count, then each as a varint tick, the combined
/// hash (u64), a section count (u8) and each section's hash (u64).
///
/// Player slots beyond the header's player count are not written and decode
/// as zero, which is what an `InputQueue` gives them.
std::vector<std::byte> encodeReplay(const Replay& replay);

/// Decodes what `encodeReplay` wrote. Never reads past `bytes`, and rejects
/// anything it cannot fully account for — including trailing bytes.
std::expected<Replay, ReplayDecodeError>
decodeReplay(std::span<const std::byte> bytes);

}  // namespace eng::sim
