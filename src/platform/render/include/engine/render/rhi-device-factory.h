#pragma once

#include <engine/render/render-config.h>
#include <engine/render/rhi-device.h>
#include <memory>
#include <optional>

namespace eng::render {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// RhiDeviceFactory: Construct the RHI device for the compiled-in backend
// (ENGINE_RENDERER_*).  Implementation lives under
// platform/render/backends/.  Backend headers are included only in the
// factory .cpp, keeping platform SDK headers out of engine/ and game/.
//
// Moved from engine/render/ to platform/render/ so that engine/ retains
// only the abstract RhiDevice interface and data types — no platform SDK
// dependencies.
//
// Threading: main thread only.
// ============================================================================

/// Result type for device creation — empty optional on failure.
using RhiDeviceOptional = std::optional<std::unique_ptr<eng::RhiDevice>>;

struct RhiDeviceFactory {
  /// Create the RHI device for the primary compiled-in renderer.
  /// Returns empty optional if backend creation fails.
  static RhiDeviceOptional create(const eng::RenderConfig& config);
};

}  // namespace eng::render
