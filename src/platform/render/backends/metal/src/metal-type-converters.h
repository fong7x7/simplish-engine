#pragma once

#ifdef ENGINE_RENDERER_METAL

// Internal header — converts RHI enums to Metal equivalents.
// Not included by any public header.

#import <Metal/Metal.h>  // NOLINT(clang-diagnostic-import-preprocessor-directive-pedantic) — Objective-C++ requires #import for Metal framework
#include <engine/render/rhi-types.h>

namespace eng {

/// Map RhiPrimitiveTopology to MTLPrimitiveType.
inline MTLPrimitiveType toMtlPrimitiveType(RhiPrimitiveTopology t) {
  switch (t) {
    case RhiPrimitiveTopology::TRIANGLE_LIST:
      return MTLPrimitiveTypeTriangle;
    case RhiPrimitiveTopology::TRIANGLE_STRIP:
      return MTLPrimitiveTypeTriangleStrip;
    case RhiPrimitiveTopology::LINE_LIST:
      return MTLPrimitiveTypeLine;
    case RhiPrimitiveTopology::POINT_LIST:
      return MTLPrimitiveTypePoint;
  }
  return MTLPrimitiveTypeTriangle;
}

/// Map RhiLoadOp to MTLLoadAction.
inline MTLLoadAction toMtlLoadAction(RhiLoadOp op) {
  switch (op) {
    case RhiLoadOp::LOAD:
      return MTLLoadActionLoad;
    case RhiLoadOp::CLEAR:
      return MTLLoadActionClear;
    case RhiLoadOp::DONT_CARE:
      return MTLLoadActionDontCare;
  }
  return MTLLoadActionDontCare;
}

/// Map RhiIndexType to MTLIndexType.
inline MTLIndexType toMtlIndexType(RhiIndexType t) {
  switch (t) {
    case RhiIndexType::UINT16:
      return MTLIndexTypeUInt16;
    case RhiIndexType::UINT32:
      return MTLIndexTypeUInt32;
  }
  return MTLIndexTypeUInt16;
}

/// Map RhiVertexAttribute format to MTLVertexFormat.
/// Named algorithm — vertex format mapping table.
inline MTLVertexFormat toMtlVertexFormat(RhiFormat fmt) {
  switch (fmt) {
    case RhiFormat::R32_FLOAT:
      return MTLVertexFormatFloat;
    case RhiFormat::R_G32_FLOAT:
      return MTLVertexFormatFloat2;
    case RhiFormat::RGB_A32_FLOAT:
      return MTLVertexFormatFloat4;
    case RhiFormat::R16_FLOAT:
      return MTLVertexFormatHalf;
    case RhiFormat::R_G16_FLOAT:
      return MTLVertexFormatHalf2;
    case RhiFormat::RGB_A16_FLOAT:
      return MTLVertexFormatHalf4;
    case RhiFormat::R8_UNORM:
      return MTLVertexFormatUCharNormalized;
    case RhiFormat::R_G8_UNORM:
      return MTLVertexFormatUChar2Normalized;
    case RhiFormat::RGB_A8_UNORM:
    case RhiFormat::RGB_A8_SRGB:
      return MTLVertexFormatUChar4Normalized;
    default:
      return MTLVertexFormatFloat4;
  }
}

enum class MtlTriangleFill : uint8_t {
  FILLED,
  WIREFRAME,
};

enum class MtlBackFaceCull : uint8_t {
  NONE,
  CULL_BACK,
};

enum class MtlFrontFaceWinding : uint8_t {
  CLOCKWISE,
  COUNTER_CLOCKWISE,
};

/// Map RhiRasterState::wireframe to MTLTriangleFillMode.
inline MTLTriangleFillMode toMtlFillMode(MtlTriangleFill fill) {
  return fill == MtlTriangleFill::WIREFRAME ? MTLTriangleFillModeLines
                                            : MTLTriangleFillModeFill;
}

/// Map RhiRasterState::cull_back to MTLCullMode.
inline MTLCullMode toMtlCullMode(MtlBackFaceCull cull) {
  return cull == MtlBackFaceCull::CULL_BACK ? MTLCullModeBack : MTLCullModeNone;
}

/// Map RhiRasterState::front_ccw to MTLWinding.
inline MTLWinding toMtlWinding(MtlFrontFaceWinding winding) {
  return winding == MtlFrontFaceWinding::COUNTER_CLOCKWISE
             ? MTLWindingCounterClockwise
             : MTLWindingClockwise;
}

}  // namespace eng

#endif  // ENGINE_RENDERER_METAL
