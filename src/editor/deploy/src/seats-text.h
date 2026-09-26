#pragma once

/// @file seats-text.h
/// @brief Seats named for a person to read.
/// @par Threading Pure functions.

#include <cstdint>
#include <string>

namespace eng::editor {

/// Who @p slot is: "player 2", or "the server" for `NET_SERVER_SLOT`.
[[nodiscard]] std::string seatText(uint8_t slot);

/// The seats in @p mask: "player 2", "players 1 and 3", "players 1, 2 and
/// 4"; empty for none.
[[nodiscard]] std::string seatsText(uint8_t mask);

}  // namespace eng::editor
