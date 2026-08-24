#pragma once

#include <engine/core/event-bus.h>
#include <string>

namespace eng {

struct EpicConfig {
  /// EOS product identifier.
  std::string product_id;
  /// Environment selector (dev/staging/live).
  std::string sandbox_id;
  /// Deployment within sandbox.
  std::string deployment_id;
  /// OAuth client ID.
  std::string client_id;
  /// OAuth client secret.
  std::string client_secret;
  /// 64-char hex key for player data encryption.
  std::string encryption_key;
  /// Non-owning pointer to the engine event bus.
  EventBus* event_bus = nullptr;
};

}  // namespace eng
