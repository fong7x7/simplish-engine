#pragma once

#include <cstdint>
#include <engine/render/rhi-core-types.h>
#include <vector>

namespace eng::render {

/// Internal D3D12 resource data for a shader handle.
struct Dx12Shader {
  /// DXIL bytecode (copied at creation time).
  std::vector<uint8_t> bytecode;
  /// Which shader stage this bytecode targets.
  RhiShaderStage stage = RhiShaderStage::VERTEX;
};

}  // namespace eng::render
