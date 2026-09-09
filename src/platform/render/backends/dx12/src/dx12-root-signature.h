#pragma once

#ifdef ENGINE_RENDERER_DX12

/// @file dx12-root-signature.h
/// @brief The one root signature every DX12 pipeline in this backend shares.
/// @par Threading Stateless; the creation calls are main-thread-only.

#include <cstdint>
#include <d3d12.h>

namespace eng::render {

// ---------------------------------------------------------------------------
// Why one shared signature
//
// The RHI has no descriptor-set concept yet: a draw passes its constants
// through `setVertexStageBytes` / `setFragmentStageBytes` and its texture
// through `bindFragmentTexture`, each addressed by a small integer slot.
// That is a fixed shape, so it is described once here and every graphics
// pipeline — the built-in GUI and mesh ones included — is created against
// it. Pipelines built from caller-supplied bytecode get it too, which is
// strictly more usable than the empty signature they had before.
// ---------------------------------------------------------------------------

/// Root parameter holding the vertex stage's slot-0 constant buffer (`b0`).
inline constexpr uint32_t DX12_ROOT_PARAM_VERTEX_CBV0 = 0;

/// Root parameter holding the vertex stage's slot-1 constant buffer (`b1`).
inline constexpr uint32_t DX12_ROOT_PARAM_VERTEX_CBV1 = 1;

/// Root parameter holding the pixel stage's slot-0 constant buffer (`b0`).
inline constexpr uint32_t DX12_ROOT_PARAM_PIXEL_CBV0 = 2;

/// Root parameter holding the pixel stage's slot-1 constant buffer (`b1`).
inline constexpr uint32_t DX12_ROOT_PARAM_PIXEL_CBV1 = 3;

/// Root parameter holding the pixel stage's sampled texture table (`t0`).
inline constexpr uint32_t DX12_ROOT_PARAM_PIXEL_SRV_TABLE = 4;

/// Returned for a stage-bytes slot this signature has no room for.
inline constexpr uint32_t DX12_ROOT_PARAM_NONE = UINT32_MAX;

/// Number of root parameters in the shared graphics signature.
inline constexpr uint32_t DX12_GRAPHICS_ROOT_PARAM_COUNT = 5;

/// Root parameter index for a vertex-stage slot, or `DX12_ROOT_PARAM_NONE`.
inline uint32_t dx12VertexCbvRootParam(uint32_t slot) {
  return slot == 0   ? DX12_ROOT_PARAM_VERTEX_CBV0
         : slot == 1 ? DX12_ROOT_PARAM_VERTEX_CBV1
                     : DX12_ROOT_PARAM_NONE;
}

/// Root parameter index for a fragment-stage slot, or `DX12_ROOT_PARAM_NONE`.
inline uint32_t dx12PixelCbvRootParam(uint32_t slot) {
  return slot == 0   ? DX12_ROOT_PARAM_PIXEL_CBV0
         : slot == 1 ? DX12_ROOT_PARAM_PIXEL_CBV1
                     : DX12_ROOT_PARAM_NONE;
}

/// Create the shared graphics root signature. Null on failure.
ID3D12RootSignature* createDx12GraphicsRootSignature(ID3D12Device5* device);

/// Create the shared (parameterless) compute root signature. Null on failure.
ID3D12RootSignature* createDx12ComputeRootSignature(ID3D12Device5* device);

}  // namespace eng::render

#endif  // ENGINE_RENDERER_DX12
