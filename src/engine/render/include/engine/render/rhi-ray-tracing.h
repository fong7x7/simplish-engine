#pragma once

/// @file rhi-ray-tracing.h
/// @brief Stub interface for hardware ray tracing (M2/M3 milestone).
/// @threading Main-thread only.

namespace eng {

/// Hardware ray tracing extension interface.
/// Returned by RhiDevice::rayTracing() — nullptr if unsupported.
/// @thread_safety Main-thread only.
class IRhiRayTracing {
public:
  virtual ~IRhiRayTracing() = default;

  IRhiRayTracing(const IRhiRayTracing&) = delete;
  IRhiRayTracing& operator=(const IRhiRayTracing&) = delete;

protected:
  IRhiRayTracing() = default;
};

}  // namespace eng
