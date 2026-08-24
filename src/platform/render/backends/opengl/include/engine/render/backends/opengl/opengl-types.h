#pragma once

#ifdef ENGINE_RENDERER_OPENGL

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// OpenGL type mappings and format conversion utilities.
//
// Responsibilities:
// - Convert RhiFormat to OpenGL internal format / format / type triplets
// - Convert RhiPrimitiveTopology to GLenum
// - Convert RhiBufferUsage to GL storage flags
// - Convert RhiVertexAttribute format to GL component info
//
// Key Invariants:
// - Pure utility; no state, no side effects
// - All functions are static members of GlFormatInfo or standalone constexpr
// - Returns safe defaults for unknown enums
//
// Threading:
// - Stateless; safe to call from any thread
// ============================================================================

#include <cstdint>
#include <engine/render/rhi-types.h>

// Forward-declare GL types to avoid requiring glad in this header.
using GLenum = unsigned int;
using GLint = int;

namespace eng::render {

/// Whether a GL buffer is host-visible (CPU-mappable) or device-local.
enum class GlHostVisibility : uint8_t {
  DEVICE_LOCAL,
  HOST_VISIBLE,
};

/// OpenGL internal format, pixel format, and component type for a given
/// RhiFormat.
struct GlFormatInfo {
  /// GL internal format (e.g. GL_RGBA8).
  GLint internal_format = 0;
  /// GL pixel data format (e.g. GL_RGBA).
  GLenum format = 0;
  /// GL component data type (e.g. GL_UNSIGNED_BYTE).
  GLenum type = 0;

  /// Convert an RhiFormat to its OpenGL format triplet.
  static GlFormatInfo fromRhiFormat(RhiFormat rhi_format);

  /// Return the number of components for a vertex attribute format.
  static uint32_t componentCount(RhiFormat rhi_format);

  /// Return the GL component type for a vertex attribute format.
  static GLenum componentType(RhiFormat rhi_format);

  /// Uncompressed texel size for pixel upload paths (0 for compressed / N/A).
  static uint32_t bytesPerTexel(RhiFormat rhi_format);
};

/// Convert RhiPrimitiveTopology to the equivalent GLenum.
GLenum toGlTopology(RhiPrimitiveTopology topology);

/// Convert RhiBufferUsage flags to GL buffer storage flags bitmask.
uint32_t toGlStorageFlags(RhiBufferUsage usage, GlHostVisibility visibility);

/// Convert RhiIndexType to the equivalent GLenum.
GLenum toGlIndexType(RhiIndexType index_type);

/// Return the byte size of one index element for the given RhiIndexType.
uint32_t indexTypeSize(RhiIndexType index_type);

}  // namespace eng::render

#endif  // ENGINE_RENDERER_OPENGL
