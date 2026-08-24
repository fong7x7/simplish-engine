#pragma once

#include "xbox-user-id.h"

namespace eng {

/// Save data context. Actual XGameSaveProvider handle stored in .cpp.
struct XboxSaveContext {
  /// True once the XGameSaveProvider is ready for read/write.
  bool provider_valid = false;
  /// Xbox user ID that owns this save provider.
  XboxUserId user_id = XBOX_USER_ID_INVALID;
};

}  // namespace eng
