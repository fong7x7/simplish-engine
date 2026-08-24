#pragma once

#include "rhi-core-types.h"

#include <cstdint>

namespace eng {

struct RhiShaderDesc {
  /// Pipeline stage this shader targets (vertex, fragment, compute).
  RhiShaderStage stage = RhiShaderStage::VERTEX;
  /// Pointer to compiled shader bytecode (SPIR-V, DXIL, etc.).
  const uint8_t* bytecode = nullptr;
  /// Size of the bytecode blob in bytes.
  uint64_t bytecode_size = 0;
  /// Shader entry point function name.
  const char* entry_point = "main";
  /// Optional debug label shown in GPU profilers.
  const char* debug_name = nullptr;
};

}  // namespace eng
