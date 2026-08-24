#include <engine/render/backends/opengl/opengl-types.h>

#ifdef ENGINE_RENDERER_OPENGL

// GL constant definitions used by the conversion functions.
// These match the OpenGL 4.6 Core Profile specification.
// Defined here to avoid requiring glad in compilation units that only need
// the conversion utilities (e.g., tests).
// NOLINTNEXTLINE(readability-identifier-naming) — FP without GL headers
constexpr GLenum GL_UNSIGNED_BYTE_C = 0x1401;
constexpr GLenum GL_UNSIGNED_SHORT_C = 0x1403;
constexpr GLenum GL_UNSIGNED_INT_C = 0x1405;
constexpr GLenum GL_FLOAT_C = 0x1406;
constexpr GLenum GL_HALF_FLOAT_C = 0x140B;

// Internal formats
constexpr GLint GL_R8_C = 0x8229;
constexpr GLint GL_RG8_C = 0x822B;
constexpr GLint GL_RGBA8_C = 0x8058;
constexpr GLint GL_SRGB8_ALPHA8_C = 0x8C43;
constexpr GLint GL_R16F_C = 0x822D;
constexpr GLint GL_RG16F_C = 0x822F;
constexpr GLint GL_RGBA16F_C = 0x881A;
constexpr GLint GL_R32F_C = 0x822E;
constexpr GLint GL_RG32F_C = 0x8230;
constexpr GLint GL_RGB32F_C = 0x8815;
constexpr GLint GL_RGBA32F_C = 0x8814;
constexpr GLint GL_DEPTH_COMPONENT16_C = 0x81A5;
constexpr GLint GL_DEPTH24_STENCIL8_C = 0x88F0;
constexpr GLint GL_DEPTH_COMPONENT32F_C = 0x8CAC;
constexpr GLint GL_DEPTH32F_STENCIL8_C = 0x8CAD;
constexpr GLint GL_COMPRESSED_RGBA_BPTC_UNORM_C = 0x8E8C;
constexpr GLint GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_C = 0x8E8D;
constexpr GLint GL_COMPRESSED_RGBA_ASTC_4X4_C = 0x93B0;
constexpr GLint GL_COMPRESSED_SRGBA_ASTC_4X4_C = 0x93D0;

// Pixel data formats
constexpr GLenum GL_RED_C = 0x1903;
constexpr GLenum GL_RG_C = 0x8227;
constexpr GLenum GL_RGB_C = 0x1907;
constexpr GLenum GL_RGBA_C = 0x1908;
constexpr GLenum GL_BGRA_C = 0x80E1;
constexpr GLenum GL_DEPTH_COMPONENT_C = 0x1902;
constexpr GLenum GL_DEPTH_STENCIL_C = 0x84F9;

// Primitive topology
constexpr GLenum GL_POINTS_C = 0x0000;
constexpr GLenum GL_LINES_C = 0x0001;
constexpr GLenum GL_TRIANGLES_C = 0x0004;
constexpr GLenum GL_TRIANGLE_STRIP_C = 0x0005;

// Buffer storage flags
constexpr uint32_t GL_MAP_READ_BIT_C = 0x0001;
constexpr uint32_t GL_MAP_WRITE_BIT_C = 0x0002;
constexpr uint32_t GL_MAP_PERSISTENT_BIT_C = 0x0040;
constexpr uint32_t GL_MAP_COHERENT_BIT_C = 0x0080;
constexpr uint32_t GL_DYNAMIC_STORAGE_BIT_C = 0x0100;

// Unsigned int type for stencil readback
constexpr GLenum GL_UNSIGNED_INT_24_8_C = 0x84FA;
constexpr GLenum GL_FLOAT_32_UNSIGNED_INT_24_8_C = 0x8DAD;

namespace eng::render {

// Named algorithm: exhaustive RhiFormat → GL format triplet lookup.
// Pure mapping with no side effects; covers all RhiFormat enum values.
GlFormatInfo GlFormatInfo::fromRhiFormat(RhiFormat rhi_format) {
  switch (rhi_format) {
    case RhiFormat::R8_UNORM:
      return {GL_R8_C, GL_RED_C, GL_UNSIGNED_BYTE_C};
    case RhiFormat::R_G8_UNORM:
      return {GL_RG8_C, GL_RG_C, GL_UNSIGNED_BYTE_C};
    case RhiFormat::RGB_A8_UNORM:
      return {GL_RGBA8_C, GL_RGBA_C, GL_UNSIGNED_BYTE_C};
    case RhiFormat::RGB_A8_SRGB:
      return {GL_SRGB8_ALPHA8_C, GL_RGBA_C, GL_UNSIGNED_BYTE_C};
    case RhiFormat::BGR_A8_UNORM:
      return {GL_RGBA8_C, GL_BGRA_C, GL_UNSIGNED_BYTE_C};
    case RhiFormat::BGR_A8_SRGB:
      return {GL_SRGB8_ALPHA8_C, GL_BGRA_C, GL_UNSIGNED_BYTE_C};
    case RhiFormat::R16_FLOAT:
      return {GL_R16F_C, GL_RED_C, GL_HALF_FLOAT_C};
    case RhiFormat::R_G16_FLOAT:
      return {GL_RG16F_C, GL_RG_C, GL_HALF_FLOAT_C};
    case RhiFormat::RGB_A16_FLOAT:
      return {GL_RGBA16F_C, GL_RGBA_C, GL_HALF_FLOAT_C};
    case RhiFormat::R32_FLOAT:
      return {GL_R32F_C, GL_RED_C, GL_FLOAT_C};
    case RhiFormat::R_G32_FLOAT:
      return {GL_RG32F_C, GL_RG_C, GL_FLOAT_C};
    case RhiFormat::R_G_B32_FLOAT:
      return {GL_RGB32F_C, GL_RGB_C, GL_FLOAT_C};
    case RhiFormat::RGB_A32_FLOAT:
      return {GL_RGBA32F_C, GL_RGBA_C, GL_FLOAT_C};
    case RhiFormat::D16_UNORM:
      return {GL_DEPTH_COMPONENT16_C, GL_DEPTH_COMPONENT_C,
              GL_UNSIGNED_SHORT_C};
    case RhiFormat::D24_UNORM_S8_UINT:
      return {GL_DEPTH24_STENCIL8_C, GL_DEPTH_STENCIL_C,
              GL_UNSIGNED_INT_24_8_C};
    case RhiFormat::D32_FLOAT:
      return {GL_DEPTH_COMPONENT32F_C, GL_DEPTH_COMPONENT_C, GL_FLOAT_C};
    case RhiFormat::D32_FLOAT_S8_UINT:
      return {GL_DEPTH32F_STENCIL8_C, GL_DEPTH_STENCIL_C,
              GL_FLOAT_32_UNSIGNED_INT_24_8_C};
    case RhiFormat::B_C7_UNORM:
      return {GL_COMPRESSED_RGBA_BPTC_UNORM_C, GL_RGBA_C, 0};
    case RhiFormat::B_C7_SRGB:
      return {GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_C, GL_RGBA_C, 0};
    case RhiFormat::ASTC4X4_UNORM:
      return {GL_COMPRESSED_RGBA_ASTC_4X4_C, GL_RGBA_C, 0};
    case RhiFormat::ASTC4X4_SRGB:
      return {GL_COMPRESSED_SRGBA_ASTC_4X4_C, GL_RGBA_C, 0};
    case RhiFormat::UNDEFINED:
      return {0, 0, 0};
  }
  return {0, 0, 0};
}

// Named algorithm: RhiFormat → byte size for glTexSubImage2D-style uploads.
uint32_t GlFormatInfo::bytesPerTexel(RhiFormat rhi_format) {
  switch (rhi_format) {
    case RhiFormat::UNDEFINED:
      return 0;
    case RhiFormat::R8_UNORM:
      return 1;
    case RhiFormat::R_G8_UNORM:
      return 2;
    case RhiFormat::R16_FLOAT:
      return 2;
    case RhiFormat::R_G16_FLOAT:
      return 4;
    case RhiFormat::RGB_A8_UNORM:
    case RhiFormat::RGB_A8_SRGB:
    case RhiFormat::BGR_A8_UNORM:
    case RhiFormat::BGR_A8_SRGB:
      return 4;
    case RhiFormat::RGB_A16_FLOAT:
      return 8;
    case RhiFormat::R32_FLOAT:
      return 4;
    case RhiFormat::R_G32_FLOAT:
      return 8;
    case RhiFormat::R_G_B32_FLOAT:
      return 12;
    case RhiFormat::RGB_A32_FLOAT:
      return 16;
    case RhiFormat::D16_UNORM:
      return 2;
    case RhiFormat::D24_UNORM_S8_UINT:
      return 4;
    case RhiFormat::D32_FLOAT:
      return 4;
    case RhiFormat::D32_FLOAT_S8_UINT:
      return 8;
    case RhiFormat::B_C7_UNORM:
    case RhiFormat::B_C7_SRGB:
    case RhiFormat::ASTC4X4_UNORM:
    case RhiFormat::ASTC4X4_SRGB:
      return 0;
  }
  return 0;
}

// Named algorithm: RhiFormat → GL vertex attribute component count.
// Pure lookup with no side effects.
uint32_t GlFormatInfo::componentCount(RhiFormat rhi_format) {
  switch (rhi_format) {
    case RhiFormat::R8_UNORM:
    case RhiFormat::R16_FLOAT:
    case RhiFormat::R32_FLOAT:
      return 1;
    case RhiFormat::R_G8_UNORM:
    case RhiFormat::R_G16_FLOAT:
    case RhiFormat::R_G32_FLOAT:
      return 2;
    case RhiFormat::RGB_A8_UNORM:
    case RhiFormat::RGB_A8_SRGB:
    case RhiFormat::BGR_A8_UNORM:
    case RhiFormat::BGR_A8_SRGB:
    case RhiFormat::RGB_A16_FLOAT:
    case RhiFormat::RGB_A32_FLOAT:
      return 4;
    default:
      return 4;
  }
}

// Named algorithm: RhiFormat → GL vertex attribute component type.
// Pure lookup with no side effects.
GLenum GlFormatInfo::componentType(RhiFormat rhi_format) {
  switch (rhi_format) {
    case RhiFormat::R8_UNORM:
    case RhiFormat::R_G8_UNORM:
    case RhiFormat::RGB_A8_UNORM:
    case RhiFormat::RGB_A8_SRGB:
    case RhiFormat::BGR_A8_UNORM:
    case RhiFormat::BGR_A8_SRGB:
      return GL_UNSIGNED_BYTE_C;
    case RhiFormat::R16_FLOAT:
    case RhiFormat::R_G16_FLOAT:
    case RhiFormat::RGB_A16_FLOAT:
      return GL_HALF_FLOAT_C;
    case RhiFormat::R32_FLOAT:
    case RhiFormat::R_G32_FLOAT:
    case RhiFormat::RGB_A32_FLOAT:
      return GL_FLOAT_C;
    default:
      return GL_FLOAT_C;
  }
}

GLenum toGlTopology(RhiPrimitiveTopology topology) {
  switch (topology) {
    case RhiPrimitiveTopology::TRIANGLE_LIST:
      return GL_TRIANGLES_C;
    case RhiPrimitiveTopology::TRIANGLE_STRIP:
      return GL_TRIANGLE_STRIP_C;
    case RhiPrimitiveTopology::LINE_LIST:
      return GL_LINES_C;
    case RhiPrimitiveTopology::POINT_LIST:
      return GL_POINTS_C;
  }
  return GL_TRIANGLES_C;
}

uint32_t toGlStorageFlags(RhiBufferUsage /*usage*/,
                          GlHostVisibility visibility) {
  if (visibility == GlHostVisibility::HOST_VISIBLE) {
    return GL_MAP_READ_BIT_C | GL_MAP_WRITE_BIT_C | GL_MAP_PERSISTENT_BIT_C |
           GL_MAP_COHERENT_BIT_C | GL_DYNAMIC_STORAGE_BIT_C;
  }
  return GL_DYNAMIC_STORAGE_BIT_C;
}

GLenum toGlIndexType(RhiIndexType index_type) {
  switch (index_type) {
    case RhiIndexType::UINT16:
      return GL_UNSIGNED_SHORT_C;
    case RhiIndexType::UINT32:
      return GL_UNSIGNED_INT_C;
  }
  return GL_UNSIGNED_SHORT_C;
}

uint32_t indexTypeSize(RhiIndexType index_type) {
  switch (index_type) {
    case RhiIndexType::UINT16:
      return 2;
    case RhiIndexType::UINT32:
      return 4;
  }
  return 2;
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_OPENGL
