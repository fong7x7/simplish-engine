#include <bit>
#include <cstring>
#include <engine/sim/state-hasher.h>

namespace eng::sim {

static_assert(std::endian::native == std::endian::little,
              "tick hashes are defined over little-endian bytes; a big-endian "
              "target needs byte swaps in loadWord before it can join a "
              "session");

namespace {

  /// Bytes folded in per step.
  constexpr std::size_t WORD_BYTES = sizeof(uint64_t);

  /// Added after every step so an all-zero state cannot stay all-zero.
  constexpr uint64_t ROUND_CONSTANT = 0x632BE59BD9B4E019ULL;

  /// MurmurHash3's 64-bit finaliser: a bijection that spreads every input
  /// bit across the output.
  uint64_t mix(uint64_t x) {
    x ^= x >> 33U;
    x *= 0xFF51AFD7ED558CCDULL;
    x ^= x >> 33U;
    x *= 0xC4CEB9FE1A85EC53ULL;
    x ^= x >> 33U;
    return x;
  }

  /// Up to eight bytes as a little-endian word, zero-filled past the end.
  uint64_t loadWord(std::span<const std::byte> bytes) {
    uint64_t word = 0;
    std::memcpy(&word, bytes.data(), bytes.size());
    return word;
  }

}  // namespace

void StateHasher::addBytes(std::span<const std::byte> bytes) {
  std::size_t offset = 0;
  for (; offset + WORD_BYTES <= bytes.size(); offset += WORD_BYTES) {
    state_ = mix(state_ ^ loadWord(bytes.subspan(offset, WORD_BYTES))) +
             ROUND_CONSTANT;
  }
  if (offset < bytes.size()) {
    state_ = mix(state_ ^ loadWord(bytes.subspan(offset))) + ROUND_CONSTANT;
  }
  // The length, so {1, 2} then {} hashes differently from {1} then {2}.
  state_ = mix(state_ ^ static_cast<uint64_t>(bytes.size())) + ROUND_CONSTANT;
}

}  // namespace eng::sim
