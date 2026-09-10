#pragma once

#ifdef ENGINE_RENDERER_DX12

/// @file dx12-builtin-pipelines.h
/// @brief The GUI and static-mesh pipeline states the backend ships itself.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>

namespace eng::render {

// ---------------------------------------------------------------------------
// These exist for the same reason the Metal backend compiles MSL at
// device creation: `createShader` takes compiled bytecode and the project
// has no shader build step yet, so the GUI and the mesh renderer would have
// nothing to draw with. The HLSL lives in the .cpp beside them and mirrors
// `GUI_MSL_SOURCE` / `MESH_MSL_SOURCE` in `metal-device-impl.mm` — the two
// are meant to shade alike, so a change to one belongs in the other.
// ---------------------------------------------------------------------------

/// Compile the built-in GUI shaders and create their PSO. Null on failure.
ID3D12PipelineState* createDx12GuiPipelineState(ID3D12Device5* device,
                                                ID3D12RootSignature* root_sig,
                                                DXGI_FORMAT color_format);

/// Compile the built-in mesh shaders and create their PSO. Null on failure.
ID3D12PipelineState* createDx12MeshPipelineState(ID3D12Device5* device,
                                                 ID3D12RootSignature* root_sig,
                                                 DXGI_FORMAT color_format);

/// Compile the built-in mesh outline shaders and create their PSO, which
/// takes no vertex input and no depth attachment. Null on failure.
ID3D12PipelineState*
createDx12OutlinePipelineState(ID3D12Device5* device,
                               ID3D12RootSignature* root_sig,
                               DXGI_FORMAT color_format);

/// Byte stride the GUI pipeline's vertex buffer is bound with.
uint32_t dx12GuiVertexStride();

/// Byte stride the mesh pipeline's vertex buffer is bound with.
uint32_t dx12MeshVertexStride();

}  // namespace eng::render

#endif  // ENGINE_RENDERER_DX12
