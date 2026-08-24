#pragma once

#include "frame-timings-config.h"

#include <string_view>

namespace eng {

/// Load `FrameTimingsConfig` from a JSON file at `path`.
///
/// Returns a default-valued config (with `enabled = true` and
/// `ring_capacity = 600`) when the file is missing, unparseable, or contains
/// malformed fields. Individual missing fields fall back to their defaults;
/// the returned config is always valid.
///
/// Thread Safety: Safe to call from any thread during init. Reads the file
/// synchronously; no hidden shared state. Call exactly once per engine
/// lifetime, typically during the core init phase.
FrameTimingsConfig loadFrameTimingsConfig(std::string_view path);

}  // namespace eng
