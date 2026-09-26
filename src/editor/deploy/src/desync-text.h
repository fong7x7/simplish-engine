#pragma once

/// @file desync-text.h
/// @brief A halted co-op run's desync, said in words.
/// @par Threading Pure functions.

#include <deque>
#include <engine/net/net-desync.h>
#include <engine/sim/tick-hash.h>
#include <string>

namespace eng::editor {

/// Checkpoint hashes a peer keeps to name a desync's section by: as many
/// as the server compares.
inline constexpr std::size_t DEPLOYED_CHECKPOINTS = 16;

/// @p desync as a sentence: the tick, the section — named from whichever
/// of @p checkpoints, this peer's own recent hashes, was taken at that
/// tick, or by number when none was — and who disagreed.
[[nodiscard]] std::string
describeDesync(const net::NetDesync& desync,
               const std::deque<sim::TickHash>& checkpoints);

/// Keep @p hash in @p checkpoints when its tick is one a session compares,
/// forgetting the oldest past `DEPLOYED_CHECKPOINTS`.
void keepCheckpoint(const sim::TickHash& hash,
                    std::deque<sim::TickHash>& checkpoints);

}  // namespace eng::editor
