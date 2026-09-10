#pragma once

/// @file mesh-gpu-upload.h
/// @brief Copying mesh geometry from CPU memory into GPU buffers, shared by
/// the static and the skinned mesh renderers.
/// @par Threading Main-thread only.

#include <cstdint>
#include <engine/render/rhi-buffer-desc.h>
#include <engine/render/rhi-core-types.h>
#include <engine/render/rhi-device.h>

namespace eng {

/// Create a host-visible buffer of @p bytes and fill it from @p data.
/// Zero when either step fails, with nothing left allocated.
[[nodiscard]] RhiBufferHandle uploadMeshBuffer(RhiDevice& device,
                                               const void* data, uint64_t bytes,
                                               RhiBufferUsage usage);

}  // namespace eng
