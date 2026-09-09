#pragma once

#ifdef ENGINE_RENDERER_DX12

#include "dx12-pipeline-type.h"

#include <cstdint>
#include <d3d12.h>

namespace eng::render {

/// Internal D3D12 resource data for a pipeline handle.
struct Dx12Pipeline {
  /// D3D12 pipeline state object.
  ID3D12PipelineState* pipeline_state = nullptr;
  /// Root signature this pipeline was created against. Borrowed from the
  /// device, which creates one of each kind and shares them, so destroying
  /// a pipeline must not release it.
  ID3D12RootSignature* root_signature = nullptr;
  /// Whether this is a graphics or compute pipeline.
  Dx12PipelineType bind_point = Dx12PipelineType::GRAPHICS;
  /// Input assembler topology for draw calls (graphics only).
  D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
  /// Vertex buffer stride in bytes (graphics only).
  uint32_t vertex_stride = 0;
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_DX12
