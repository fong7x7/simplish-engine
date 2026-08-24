#pragma once

#include <cstdint>

namespace eng::render {

/// Distinguishes graphics from compute pipelines in the DX12 backend.
enum class Dx12PipelineType : uint8_t {
  GRAPHICS,
  COMPUTE,
};

}  // namespace eng::render
