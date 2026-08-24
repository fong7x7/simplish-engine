#pragma once

#ifdef ENGINE_RENDERER_METAL

// Internal header — maps RhiFormat to MTLPixelFormat.
// Not included by any public header.

#import <Metal/Metal.h>  // NOLINT(clang-diagnostic-import-preprocessor-directive-pedantic) — Objective-C++ requires #import for Metal framework
#include <engine/render/rhi-types.h>

namespace eng {

/// Map RhiFormat to the corresponding MTLPixelFormat.
/// Returns MTLPixelFormatInvalid for UNDEFINED.
/// Named algorithm — format mapping table (~30 cases).
inline MTLPixelFormat toMtlPixelFormat(RhiFormat fmt) {
  switch (fmt) {
    case RhiFormat::UNDEFINED:
      return MTLPixelFormatInvalid;
    case RhiFormat::R8_UNORM:
      return MTLPixelFormatR8Unorm;
    case RhiFormat::R_G8_UNORM:
      return MTLPixelFormatRG8Unorm;
    case RhiFormat::RGB_A8_UNORM:
      return MTLPixelFormatRGBA8Unorm;
    case RhiFormat::RGB_A8_SRGB:
      return MTLPixelFormatRGBA8Unorm_sRGB;
    case RhiFormat::BGR_A8_UNORM:
      return MTLPixelFormatBGRA8Unorm;
    case RhiFormat::BGR_A8_SRGB:
      return MTLPixelFormatBGRA8Unorm_sRGB;
    case RhiFormat::R16_FLOAT:
      return MTLPixelFormatR16Float;
    case RhiFormat::R_G16_FLOAT:
      return MTLPixelFormatRG16Float;
    case RhiFormat::RGB_A16_FLOAT:
      return MTLPixelFormatRGBA16Float;
    case RhiFormat::R32_FLOAT:
      return MTLPixelFormatR32Float;
    case RhiFormat::R_G32_FLOAT:
      return MTLPixelFormatRG32Float;
    case RhiFormat::R_G_B32_FLOAT:
      return MTLPixelFormatInvalid;
    case RhiFormat::RGB_A32_FLOAT:
      return MTLPixelFormatRGBA32Float;
    case RhiFormat::D16_UNORM:
      return MTLPixelFormatDepth16Unorm;
    case RhiFormat::D24_UNORM_S8_UINT:
      return MTLPixelFormatDepth24Unorm_Stencil8;
    case RhiFormat::D32_FLOAT:
      return MTLPixelFormatDepth32Float;
    case RhiFormat::D32_FLOAT_S8_UINT:
      return MTLPixelFormatDepth32Float_Stencil8;
    case RhiFormat::B_C7_UNORM:
      return MTLPixelFormatBC7_RGBAUnorm;
    case RhiFormat::B_C7_SRGB:
      return MTLPixelFormatBC7_RGBAUnorm_sRGB;
    case RhiFormat::ASTC4X4_UNORM:
      return MTLPixelFormatASTC_4x4_LDR;
    case RhiFormat::ASTC4X4_SRGB:
      return MTLPixelFormatASTC_4x4_sRGB;
  }
  return MTLPixelFormatInvalid;
}

/// Bytes per texel for a given RhiFormat. Used for buffer sizing.
/// Named algorithm — format byte size table.
inline uint32_t bytesPerTexel(RhiFormat fmt) {
  switch (fmt) {
    case RhiFormat::UNDEFINED:
    case RhiFormat::B_C7_UNORM:
    case RhiFormat::B_C7_SRGB:
    case RhiFormat::ASTC4X4_UNORM:
    case RhiFormat::ASTC4X4_SRGB:
      return 0;
    case RhiFormat::R8_UNORM:
      return 1;
    case RhiFormat::R_G8_UNORM:
      return 2;
    case RhiFormat::RGB_A8_UNORM:
    case RhiFormat::RGB_A8_SRGB:
    case RhiFormat::BGR_A8_UNORM:
    case RhiFormat::BGR_A8_SRGB:
      return 4;
    case RhiFormat::R16_FLOAT:
      return 2;
    case RhiFormat::R_G16_FLOAT:
    case RhiFormat::R32_FLOAT:
    case RhiFormat::D32_FLOAT:
      return 4;
    case RhiFormat::RGB_A16_FLOAT:
    case RhiFormat::R_G32_FLOAT:
    case RhiFormat::D32_FLOAT_S8_UINT:
      return 8;
    case RhiFormat::R_G_B32_FLOAT:
      return 12;
    case RhiFormat::RGB_A32_FLOAT:
      return 16;
    case RhiFormat::D16_UNORM:
      return 2;
    case RhiFormat::D24_UNORM_S8_UINT:
      return 4;
  }
  return 0;
}

}  // namespace eng

#endif  // ENGINE_RENDERER_METAL
