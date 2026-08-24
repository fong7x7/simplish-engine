#pragma once

#include <cstdint>
#include <string_view>

namespace eng {

struct XboxSessionParams {
  /// Maximum number of players allowed in the session.
  uint32_t max_players = 8;
  /// MPSD session template name configured in Partner Center.
  std::string_view session_template_name;
};

}  // namespace eng
