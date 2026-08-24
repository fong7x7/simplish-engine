#pragma once

#include "rhi-blend-state.h"
#include "rhi-core-types.h"
#include "rhi-depth-stencil-state.h"
#include "rhi-raster-state.h"
#include "rhi-vertex-layout.h"

#include <cstdint>

namespace eng {

struct RhiGraphicsPipelineDesc {
  /// Compiled vertex shader handle.
  RhiShaderHandle vertex_shader = RHI_SHADER_INVALID;
  /// Compiled fragment shader handle.
  RhiShaderHandle fragment_shader = RHI_SHADER_INVALID;
  /// Vertex input attribute layout.
  RhiVertexLayout vertex_layout{};
  /// Primitive assembly topology.
  RhiPrimitiveTopology topology = RhiPrimitiveTopology::TRIANGLE_LIST;
  /// Colour attachment blend state.
  RhiBlendState blend{};
  /// Depth and stencil test configuration.
  RhiDepthStencilState depth_stencil{};
  /// Rasteriser state (fill mode, culling, winding).
  RhiRasterState raster{};
  /// Pixel format of the colour render target.
  RhiFormat color_format = RhiFormat::RGB_A8_SRGB;
  /// Pixel format of the depth render target.
  RhiFormat depth_format = RhiFormat::D32_FLOAT;
  /// Optional debug label shown in GPU profilers.
  const char* debug_name = nullptr;
};

}  // namespace eng
