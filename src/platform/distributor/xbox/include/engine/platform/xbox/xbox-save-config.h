#pragma once

#include "xbox-user-id.h"

namespace eng {

struct XboxSaveConfig {
  /// Xbox user ID for the Connected Storage provider.
  XboxUserId user_id = XBOX_USER_ID_INVALID;
};

}  // namespace eng
