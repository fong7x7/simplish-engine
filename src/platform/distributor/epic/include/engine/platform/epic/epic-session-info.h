#pragma once

#include <cstdint>
#include <string>

namespace eng {

struct EpicSessionInfo {
  /// Unique identifier for this EOS session.
  std::string session_id{};
  /// Human-readable name of the server hosting this session.
  std::string server_name{};
  /// Number of players currently in the session.
  uint32_t current_players = 0;
  /// Maximum number of players allowed in the session.
  uint32_t max_players = 0;
};

}  // namespace eng
