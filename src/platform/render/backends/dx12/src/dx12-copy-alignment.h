#pragma once

#ifdef ENGINE_RENDERER_DX12

/// @file dx12-copy-alignment.h
/// @brief The row pitch every D3D12 buffer-to-texture copy has to use.
/// @par Threading Stateless.

#include <cstdint>
#include <d3d12.h>

namespace eng::render {

/// Round a tightly packed row up to D3D12's placed-footprint row pitch.
///
/// A `CopyTextureRegion` whose footprint pitch is not a multiple of
/// `D3D12_TEXTURE_DATA_PITCH_ALIGNMENT` is rejected outright, so both the
/// upload path and the capture path pad their rows to it and repack around
/// the padding on the CPU side.
inline uint32_t dx12AlignRowPitch(uint32_t row_bytes) {
  return (row_bytes + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) &
         ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_DX12
