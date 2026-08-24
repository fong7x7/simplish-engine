#pragma once

#ifdef ENGINE_RENDERER_DX12

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// Dx12Device::Impl — PIMPL body for Dx12Device.
//
// Responsibilities:
// - Hold all D3D12/DXGI state (factory, device, allocator, swapchain, queues)
// - Define internal resource structs (Dx12Buffer, Dx12Texture, etc.)
// - Provide init/destroy/utility helpers called by Dx12Device methods
//
// Key Invariants:
// - This header is internal; never included from public headers
// - All D3D12 types are confined here (pImpl isolation)
// - Thread safety: main-thread-only (same as Dx12Device)
// ============================================================================

#include "dx12-buffer-resource.h"
#include "dx12-descriptor-heap-allocator.h"
#include "dx12-frames-in-flight.h"
#include "dx12-handle-table.h"
#include "dx12-per-frame-data.h"
#include "dx12-pipeline-resource.h"
#include "dx12-shader-resource.h"
#include "dx12-texture-resource.h"

#include <D3D12MemAlloc.h>
#include <array>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <engine/render/backends/dx12/dx12-device.h>
#include <engine/render/render-config.h>
#include <engine/render/rhi-device-capabilities.h>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace eng::render {

/// Maximum number of shader resource view descriptors.
constexpr uint32_t MAX_SRV_DESCRIPTORS = 4096;

/// Maximum number of render target view descriptors.
constexpr uint32_t MAX_RTV_DESCRIPTORS = 64;

/// Maximum number of depth stencil view descriptors.
constexpr uint32_t MAX_DSV_DESCRIPTORS = 32;

struct Dx12Device::Impl {
  /// DXGI factory for adapter enumeration and swapchain creation.
  IDXGIFactory6* factory = nullptr;
  /// Selected DXGI adapter (discrete GPU preferred).
  IDXGIAdapter4* adapter = nullptr;
  /// D3D12 logical device.
  ID3D12Device5* device = nullptr;
  /// Direct command queue for graphics submission.
  ID3D12CommandQueue* command_queue = nullptr;
  /// DXGI swap chain.
  IDXGISwapChain4* swapchain = nullptr;
  /// D3D12MA allocator for GPU memory.
  D3D12MA::Allocator* allocator = nullptr;
  /// Descriptor heap for render target views.
  ID3D12DescriptorHeap* rtv_heap = nullptr;
  /// Descriptor heap for depth stencil views.
  ID3D12DescriptorHeap* dsv_heap = nullptr;
  /// Shader-visible descriptor heap for CBV/SRV/UAV.
  ID3D12DescriptorHeap* cbv_srv_uav_heap = nullptr;
  /// RTV descriptor increment size.
  uint32_t rtv_increment = 0;
  /// DSV descriptor increment size.
  uint32_t dsv_increment = 0;
  /// CBV/SRV/UAV descriptor increment size.
  uint32_t cbv_srv_uav_increment = 0;
  /// SRV descriptor index allocator.
  Dx12DescriptorHeapAllocator srv_allocator;
  /// RTV descriptor index allocator.
  Dx12DescriptorHeapAllocator rtv_allocator;
  /// DSV descriptor index allocator.
  Dx12DescriptorHeapAllocator dsv_allocator;
  /// Swapchain back buffer resources.
  std::array<ID3D12Resource*, DX12_FRAMES_IN_FLIGHT> swapchain_images{};
  /// RTV handles for swapchain back buffers.
  std::array<D3D12_CPU_DESCRIPTOR_HANDLE, DX12_FRAMES_IN_FLIGHT>
      swapchain_rtvs{};
  /// Swapchain width in pixels.
  uint32_t swapchain_width = 0;
  /// Swapchain height in pixels.
  uint32_t swapchain_height = 0;
  /// Swapchain pixel format.
  DXGI_FORMAT swapchain_format = DXGI_FORMAT_UNKNOWN;
  /// Current frame index (0..DX12_FRAMES_IN_FLIGHT-1).
  uint32_t frame_index = 0;
  /// Current swapchain back buffer index.
  uint32_t image_index = 0;
  /// Per-frame synchronization and command data.
  std::array<Dx12PerFrameData, DX12_FRAMES_IN_FLIGHT> frames{};
  /// Cached device capabilities.
  RhiDeviceCapabilities caps{};
  /// Device name string (owned for lifetime stability).
  std::string device_name_str;
  /// API version string (owned for lifetime stability).
  std::string api_version_str;
  /// Render config used at creation.
  RenderConfig config{};
  /// Buffer handle table.
  Dx12HandleTable<Dx12Buffer> buffers;
  /// Texture handle table.
  Dx12HandleTable<Dx12Texture> textures;
  /// Shader handle table.
  Dx12HandleTable<Dx12Shader> shaders;
  /// Pipeline handle table.
  Dx12HandleTable<Dx12Pipeline> pipelines;
  /// Command signature for non-indexed indirect draws.
  ID3D12CommandSignature* draw_indirect_sig = nullptr;
  /// Command signature for indexed indirect draws.
  ID3D12CommandSignature* draw_indexed_indirect_sig = nullptr;
  /// Whether a swapchain resize is pending.
  bool resize_pending = false;
  /// Pending resize width.
  uint32_t pending_width = 0;
  /// Pending resize height.
  uint32_t pending_height = 0;

  // --- Init helpers ---

  /// Run all initialisation stages in order; returns false on any failure.
  bool initAll();
  /// Create the DXGI factory (with optional debug layer).
  bool initFactory();
  /// Select the best DXGI adapter (prefers discrete GPU).
  bool selectAdapter();
  /// Create the D3D12 device with feature level 12_0.
  bool initDevice();
  /// Create the direct command queue.
  bool initCommandQueue();
  /// Create the D3D12MA allocator.
  bool initAllocator();
  /// Create descriptor heaps (RTV, DSV, CBV/SRV/UAV).
  bool initDescriptorHeaps();
  /// Create the DXGI swap chain from the native window handle.
  bool initSwapchain();
  /// Allocate per-frame command allocators, command lists, and fences.
  bool initPerFrameData();
  /// Create command signatures for indirect draw calls.
  bool initCommandSignatures();
  /// Query adapter properties and fill the capabilities struct.
  void populateCapabilities();

  // --- Destroy helpers ---

  /// Destroy per-frame fences, command lists, and command allocators.
  void destroyPerFrameData();
  /// Release swapchain back buffer references and the swapchain itself.
  void destroySwapchain();
  /// Destroy descriptor heaps, allocator, command queue, device, and factory.
  void destroyCoreObjects();
  /// Full teardown: per-frame data, swapchain, and core objects.
  void teardown();

  // --- Utility helpers ---

  /// Recreate the swapchain after a resize event.
  void recreateSwapchain();
  /// Wait for all GPU work on all frames to complete.
  void waitAllFrames();
  /// Get an RTV CPU handle at the given index.
  D3D12_CPU_DESCRIPTOR_HANDLE rtvCpuHandle(uint32_t index) const;
  /// Get a DSV CPU handle at the given index.
  D3D12_CPU_DESCRIPTOR_HANDLE dsvCpuHandle(uint32_t index) const;
  /// Get a CBV/SRV/UAV CPU handle at the given index.
  D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle(uint32_t index) const;
  /// Get a CBV/SRV/UAV GPU handle at the given index.
  D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle(uint32_t index) const;
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_DX12
