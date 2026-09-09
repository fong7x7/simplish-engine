#include "dx12-device-impl.h"

#ifdef ENGINE_RENDERER_DX12

#include "dx12-root-signature.h"

#include <D3D12MemAlloc.h>
#include <SDL3/SDL.h>
#include <array>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace eng::render {

// ---------------------------------------------------------------------------
// Anonymous-namespace helpers (pure functions)
// ---------------------------------------------------------------------------

namespace {

  /// Whether D3D12 debug layer is requested.
  enum class DebugEnabled { NO, YES };

  /// Bytes of constant data one frame's `setVertexStageBytes` /
  /// `setFragmentStageBytes` calls may hand out before they start being
  /// dropped. Each push rounds up to a 256-byte CBV boundary, so this is
  /// roughly four thousand of them — far past any frame the editor records.
  constexpr uint64_t DX12_STAGE_BYTES_CAPACITY = 1024 * 1024;

  bool createDxgiFactory(DebugEnabled debug, IDXGIFactory6** out) {
    UINT flags = 0;
    if (debug == DebugEnabled::YES) {
      flags = DXGI_CREATE_FACTORY_DEBUG;
    }
    HRESULT hr = CreateDXGIFactory2(flags, IID_PPV_ARGS(out));
    return SUCCEEDED(hr);
  }

  bool enableDebugLayer() {
    ID3D12Debug* debug_ctrl = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_ctrl)))) {
      debug_ctrl->EnableDebugLayer();
      debug_ctrl->Release();
      return true;
    }
    return false;
  }

  /// Create DXGI factory with optional debug layer; retry without on failure.
  bool createFactoryWithRetry(DebugEnabled debug, IDXGIFactory6** out) {
    if (debug == DebugEnabled::YES) {
      enableDebugLayer();
    }
    if (createDxgiFactory(debug, out)) {
      return true;
    }
    if (debug == DebugEnabled::YES) {
      return createDxgiFactory(DebugEnabled::NO, out);
    }
    return false;
  }

  /// Select discrete GPU adapter via DXGI 1.6 preference.
  bool selectDiscreteAdapter(IDXGIFactory6* factory, IDXGIAdapter4** out) {
    HRESULT hr = factory->EnumAdapterByGpuPreference(
        0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(out));
    return SUCCEEDED(hr) && *out != nullptr;
  }

  bool createD3D12Device(IDXGIAdapter4* adapter, ID3D12Device5** out) {
    HRESULT hr =
        D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(out));
    return SUCCEEDED(hr);
  }

  bool createDirectCommandQueue(ID3D12Device5* device,
                                ID3D12CommandQueue** out) {
    D3D12_COMMAND_QUEUE_DESC desc{};
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    return SUCCEEDED(device->CreateCommandQueue(&desc, IID_PPV_ARGS(out)));
  }

  bool createD3D12MAAllocator(ID3D12Device5* device, IDXGIAdapter4* adapter,
                              D3D12MA::Allocator** out) {
    D3D12MA::ALLOCATOR_DESC desc{};
    desc.pDevice = device;
    desc.pAdapter = adapter;
    return SUCCEEDED(D3D12MA::CreateAllocator(&desc, out));
  }

  /// Grouped parameters for descriptor heap creation.
  struct HeapCreateParams {
    /// Heap contents (RTV, DSV or CBV/SRV/UAV).
    D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    /// Number of descriptors the heap holds.
    uint32_t count = 0;
    /// Whether shaders may read from the heap.
    D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  };

  bool createDescriptorHeap(ID3D12Device5* device, const HeapCreateParams& p,
                            ID3D12DescriptorHeap** out) {
    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = p.type;
    desc.NumDescriptors = p.count;
    desc.Flags = p.flags;
    return SUCCEEDED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(out)));
  }

  /// Extract HWND from SDL window via SDL3 properties.
  HWND extractHwnd(void* native_window) {
    auto* window = static_cast<SDL_Window*>(native_window);
    return static_cast<HWND>(
        SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                               SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
  }

  /// Grouped parameters for swapchain creation.
  struct SwapchainCreateParams {
    /// DXGI factory.
    IDXGIFactory6* factory = nullptr;
    /// Command queue for the swapchain.
    ID3D12CommandQueue* command_queue = nullptr;
    /// Target window handle.
    HWND hwnd = nullptr;
    /// Backbuffer width.
    uint32_t width = 0;
    /// Backbuffer height.
    uint32_t height = 0;
    /// Whether vsync is enabled.
    bool vsync = false;
  };

  DXGI_SWAP_CHAIN_DESC1
  buildSwapchainDesc(const SwapchainCreateParams& p) {
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = p.width;
    desc.Height = p.height;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = DX12_FRAMES_IN_FLIGHT;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    return desc;
  }

  SwapchainCreateParams buildSwapchainParams(const RenderConfig& config,
                                             IDXGIFactory6* factory,
                                             ID3D12CommandQueue* queue,
                                             HWND hwnd) {
    return {factory,
            queue,
            hwnd,
            config.backbuffer_width,
            config.backbuffer_height,
            config.vsync};
  }

  bool createSwapchain(const SwapchainCreateParams& p, IDXGISwapChain4** out) {
    auto desc = buildSwapchainDesc(p);
    IDXGISwapChain1* chain1 = nullptr;
    HRESULT hr = p.factory->CreateSwapChainForHwnd(
        p.command_queue, p.hwnd, &desc, nullptr, nullptr, &chain1);
    if (FAILED(hr) || chain1 == nullptr) {
      return false;
    }
    hr = chain1->QueryInterface(IID_PPV_ARGS(out));
    chain1->Release();
    return SUCCEEDED(hr);
  }

  /// Release a COM object and set its pointer to nullptr.
  template <typename T> void safeRelease(T*& ptr) {
    if (ptr != nullptr) {
      ptr->Release();
      ptr = nullptr;
    }
  }

  bool createCommandAllocator(ID3D12Device5* device,
                              ID3D12CommandAllocator** out) {
    return SUCCEEDED(device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(out)));
  }

  bool createGraphicsCommandList(ID3D12Device5* device,
                                 ID3D12CommandAllocator* allocator,
                                 ID3D12GraphicsCommandList** out) {
    HRESULT hr =
        device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator,
                                  nullptr, IID_PPV_ARGS(out));
    if (SUCCEEDED(hr)) {
      (*out)->Close();  // Start closed; reset in beginFrame
    }
    return SUCCEEDED(hr);
  }

  bool createFence(ID3D12Device5* device, Dx12PerFrameData& frame) {
    HRESULT hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                     IID_PPV_ARGS(&frame.fence));
    if (FAILED(hr)) {
      return false;  // NOLINT(readability-simplify-boolean-expr) — #ifdef block
                     // follows; can't collapse
    }
#ifdef _WIN32
    frame.fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (frame.fence_event == nullptr) {
      return false;
    }
#endif
    return true;
  }

  ID3D12CommandSignature*
  createIndirectSignature(ID3D12Device5* device,
                          D3D12_INDIRECT_ARGUMENT_TYPE arg_type,
                          UINT byte_stride) {
    D3D12_INDIRECT_ARGUMENT_DESC arg{};
    arg.Type = arg_type;
    D3D12_COMMAND_SIGNATURE_DESC desc{};
    desc.ByteStride = byte_stride;
    desc.NumArgumentDescs = 1;
    desc.pArgumentDescs = &arg;
    ID3D12CommandSignature* sig = nullptr;
    device->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(&sig));
    return sig;
  }

  bool initSingleFrame(ID3D12Device5* device, Dx12PerFrameData& frame) {
    if (!createCommandAllocator(device, &frame.command_allocator)) {
      return false;
    }
    if (!createGraphicsCommandList(device, frame.command_allocator,
                                   &frame.command_list)) {
      return false;
    }
    return createFence(device, frame);
  }

  void destroyFrameData(Dx12PerFrameData& frame) {
#ifdef _WIN32
    if (frame.fence_event != nullptr) {
      CloseHandle(frame.fence_event);
      frame.fence_event = nullptr;
    }
#endif
    frame.stage_bytes.destroy();
    safeRelease(frame.fence);
    safeRelease(frame.command_list);
    safeRelease(frame.command_allocator);
  }

  /// Block until `frame`'s last signalled fence value is reached.
  void waitForFrame(Dx12PerFrameData& frame) {
    if (frame.fence == nullptr ||
        frame.fence->GetCompletedValue() >= frame.fence_value) {
      return;
    }
#ifdef _WIN32
    frame.fence->SetEventOnCompletion(frame.fence_value, frame.fence_event);
    WaitForSingleObject(frame.fence_event, INFINITE);
#endif
  }

  /// Whether the device reports DXR support.
  bool queryRayTracing(ID3D12Device5* device) {
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options{};
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,
                                           &options, sizeof(options)))) {
      return false;
    }
    return options.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0;
  }

  /// UTF-8 form of a DXGI adapter description.
  std::string adapterName(const DXGI_ADAPTER_DESC1& desc) {
    std::string name;
    for (auto wide : desc.Description) {
      if (wide == 0) {
        break;
      }
      name.push_back(static_cast<char>(wide));
    }
    return name;
  }

}  // namespace

// ---------------------------------------------------------------------------
// Impl init helpers
// ---------------------------------------------------------------------------

bool Dx12Device::Impl::initAll() {
  bool ok = initFactory() && selectAdapter() && initDevice();
  ok = ok && initCommandQueue() && initAllocator();
  ok = ok && initDescriptorHeaps() && initSwapchain();
  ok = ok && initPerFrameData() && initUtilityFrame();
  ok = ok && initCommandSignatures() && initRootSignatures();
  if (ok) {
    initNullSrv();
    populateCapabilities();
  }
  return ok;
}

bool Dx12Device::Impl::initFactory() {
  auto debug = config.enable_validation ? DebugEnabled::YES : DebugEnabled::NO;
  return createFactoryWithRetry(debug, &factory);
}

bool Dx12Device::Impl::selectAdapter() {
  return selectDiscreteAdapter(factory, &adapter);
}

bool Dx12Device::Impl::initDevice() {
  return createD3D12Device(adapter, &device);
}

bool Dx12Device::Impl::initCommandQueue() {
  return createDirectCommandQueue(device, &command_queue);
}

bool Dx12Device::Impl::initAllocator() {
  return createD3D12MAAllocator(device, adapter, &allocator);
}

bool Dx12Device::Impl::initDescriptorHeaps() {
  bool ok =
      createDescriptorHeap(device,
                           {D3D12_DESCRIPTOR_HEAP_TYPE_RTV, MAX_RTV_DESCRIPTORS,
                            D3D12_DESCRIPTOR_HEAP_FLAG_NONE},
                           &rtv_heap);
  ok = ok && createDescriptorHeap(device,
                                  {D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
                                   MAX_DSV_DESCRIPTORS,
                                   D3D12_DESCRIPTOR_HEAP_FLAG_NONE},
                                  &dsv_heap);
  ok = ok && createDescriptorHeap(device,
                                  {D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                                   MAX_SRV_DESCRIPTORS,
                                   D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE},
                                  &cbv_srv_uav_heap);
  return ok && initDescriptorAllocators();
}

bool Dx12Device::Impl::initDescriptorAllocators() {
  rtv_increment =
      device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  dsv_increment =
      device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
  cbv_srv_uav_increment = device->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  srv_allocator = Dx12DescriptorHeapAllocator(MAX_SRV_DESCRIPTORS);
  rtv_allocator = Dx12DescriptorHeapAllocator(MAX_RTV_DESCRIPTORS);
  dsv_allocator = Dx12DescriptorHeapAllocator(MAX_DSV_DESCRIPTORS);
  // Reserve first RTV slots for swapchain back buffers
  for (uint32_t i = 0; i < DX12_FRAMES_IN_FLIGHT; ++i) {
    rtv_allocator.allocate();
  }
  return true;
}

bool Dx12Device::Impl::initSwapchain() {
  HWND hwnd = config.native_window != nullptr
                  ? extractHwnd(config.native_window)
                  : nullptr;
  if (hwnd == nullptr) {
    return false;
  }
  auto params = buildSwapchainParams(config, factory, command_queue, hwnd);
  if (!createSwapchain(params, &swapchain)) {
    return false;
  }
  recordSwapchainSize();
  return acquireSwapchainBuffers();
}

void Dx12Device::Impl::recordSwapchainSize() {
  swapchain_format = DXGI_FORMAT_R8G8B8A8_UNORM;
  swapchain_rtv_format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  swapchain_width = config.backbuffer_width;
  swapchain_height = config.backbuffer_height;
}

// Named algorithm: acquireSwapchainBuffers
// Fetches every back buffer, gives each an sRGB render target view in the
// heap slot reserved for it, and marks it PRESENT — the state DXGI hands a
// fresh or freshly resized swapchain back in.
bool Dx12Device::Impl::acquireSwapchainBuffers() {
  D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{};
  rtv_desc.Format = swapchain_rtv_format;
  rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
  auto heap_start = rtv_heap->GetCPUDescriptorHandleForHeapStart();
  for (uint32_t i = 0; i < DX12_FRAMES_IN_FLIGHT; ++i) {
    if (FAILED(swapchain->GetBuffer(i, IID_PPV_ARGS(&swapchain_images[i])))) {
      return false;
    }
    swapchain_rtvs[i].ptr =
        heap_start.ptr + static_cast<SIZE_T>(i) * rtv_increment;
    device->CreateRenderTargetView(swapchain_images[i], &rtv_desc,
                                   swapchain_rtvs[i]);
    swapchain_states[i] = D3D12_RESOURCE_STATE_PRESENT;
  }
  return true;
}

bool Dx12Device::Impl::initPerFrameData() {
  for (auto& frame : frames) {
    if (!initSingleFrame(device, frame)) {
      return false;
    }
    if (!frame.stage_bytes.create(allocator, DX12_STAGE_BYTES_CAPACITY)) {
      return false;
    }
  }
  return true;
}

bool Dx12Device::Impl::initUtilityFrame() {
  return initSingleFrame(device, utility);
}

bool Dx12Device::Impl::initCommandSignatures() {
  draw_indirect_sig = createIndirectSignature(
      device, D3D12_INDIRECT_ARGUMENT_TYPE_DRAW, sizeof(D3D12_DRAW_ARGUMENTS));
  if (draw_indirect_sig == nullptr) {
    return false;
  }
  draw_indexed_indirect_sig =
      createIndirectSignature(device, D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED,
                              sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
  return draw_indexed_indirect_sig != nullptr;
}

bool Dx12Device::Impl::initRootSignatures() {
  graphics_root_signature = createDx12GraphicsRootSignature(device);
  compute_root_signature = createDx12ComputeRootSignature(device);
  return graphics_root_signature != nullptr &&
         compute_root_signature != nullptr;
}

void Dx12Device::Impl::initNullSrv() {
  null_srv_index = srv_allocator.allocate();
  if (null_srv_index == DX12_DESCRIPTOR_INDEX_NONE) {
    return;
  }
  D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  desc.Texture2D.MipLevels = 1;
  device->CreateShaderResourceView(nullptr, &desc,
                                   srvCpuHandle(null_srv_index));
}

void Dx12Device::Impl::populateCapabilities() {
  DXGI_ADAPTER_DESC1 adapter_desc{};
  adapter->GetDesc1(&adapter_desc);
  device_name_str = adapterName(adapter_desc);
  api_version_str = "12.0";

  caps.backend = RhiBackend::D_X12;
  caps.compute_supported = true;
  caps.indirect_draw_supported = true;
  caps.ray_tracing_supported = queryRayTracing(device);
  // D3D12 states no single buffer limit; what actually runs out is the
  // adapter's own memory, which is the number Metal reports here too.
  caps.max_buffer_size = adapter_desc.DedicatedVideoMemory;
  caps.max_texture_dimension_2d = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
  // DX12 has no descriptor sets; the nearest thing a caller can bind is a
  // root parameter of the signature every pipeline here shares.
  caps.max_bound_descriptor_sets = DX12_GRAPHICS_ROOT_PARAM_COUNT;
  caps.device_name = device_name_str.c_str();
  caps.api_version = api_version_str.c_str();
}

// ---------------------------------------------------------------------------
// Impl destroy helpers
// ---------------------------------------------------------------------------

void Dx12Device::Impl::destroyPerFrameData() {
  for (auto& frame : frames) {
    destroyFrameData(frame);
  }
  destroyFrameData(utility);
}

void Dx12Device::Impl::destroyLiveResources() {
  buffers.forEachAlive([](Dx12Buffer& buf) {
    safeRelease(buf.resource);
    safeRelease(buf.allocation);
  });
  textures.forEachAlive([](Dx12Texture& tex) {
    safeRelease(tex.resource);
    safeRelease(tex.allocation);
  });
  pipelines.forEachAlive(
      [](Dx12Pipeline& pipe) { safeRelease(pipe.pipeline_state); });
}

void Dx12Device::Impl::destroySwapchain() {
  for (auto& img : swapchain_images) {
    safeRelease(img);
  }
  safeRelease(swapchain);
}

void Dx12Device::Impl::teardown() {
  // Order matters: everything D3D12MA allocated has to go before the
  // allocator that owns its heaps, and the per-frame rings are allocations
  // like any other.
  destroyPerFrameData();
  destroyLiveResources();
  destroySwapchain();
  destroyCoreObjects();
}

void Dx12Device::Impl::destroyCoreObjects() {
  safeRelease(compute_root_signature);
  safeRelease(graphics_root_signature);
  safeRelease(draw_indexed_indirect_sig);
  safeRelease(draw_indirect_sig);
  safeRelease(cbv_srv_uav_heap);
  safeRelease(dsv_heap);
  safeRelease(rtv_heap);
  safeRelease(allocator);
  safeRelease(command_queue);
  safeRelease(device);
  safeRelease(adapter);
  safeRelease(factory);
}

// ---------------------------------------------------------------------------
// Impl utility helpers
// ---------------------------------------------------------------------------

void Dx12Device::Impl::recreateSwapchain() {
  waitAllFrames();
  for (auto& img : swapchain_images) {
    safeRelease(img);
  }
  swapchain->ResizeBuffers(DX12_FRAMES_IN_FLIGHT, pending_width, pending_height,
                           swapchain_format, 0);
  swapchain_width = pending_width;
  swapchain_height = pending_height;
  acquireSwapchainBuffers();
  resize_pending = false;
}

void Dx12Device::Impl::beginFrameRecording() {
  auto& frame = frames[frame_index];
  waitForFrame(frame);
  image_index = swapchain->GetCurrentBackBufferIndex();
  // Only safe after the wait: the GPU is done with everything this frame's
  // allocator recorded and everything its ring handed out.
  frame.stage_bytes.reset();
  frame.command_allocator->Reset();
  frame.command_list->Reset(frame.command_allocator, nullptr);
}

void Dx12Device::Impl::waitAllFrames() {
  for (auto& frame : frames) {
    waitForFrame(frame);
  }
  waitForFrame(utility);
}

bool Dx12Device::Impl::beginUtilityCommands() {
  waitForFrame(utility);
  if (FAILED(utility.command_allocator->Reset())) {
    return false;
  }
  return SUCCEEDED(
      utility.command_list->Reset(utility.command_allocator, nullptr));
}

void Dx12Device::Impl::submitUtilityCommands() {
  utility.command_list->Close();
  std::array<ID3D12CommandList*, 1> lists{utility.command_list};
  command_queue->ExecuteCommandLists(1, lists.data());
  command_queue->Signal(utility.fence, ++utility.fence_value);
  waitForFrame(utility);
}

D3D12_CPU_DESCRIPTOR_HANDLE
Dx12Device::Impl::rtvCpuHandle(uint32_t index) const {
  auto start = rtv_heap->GetCPUDescriptorHandleForHeapStart();
  start.ptr += static_cast<SIZE_T>(index) * rtv_increment;
  return start;
}

D3D12_CPU_DESCRIPTOR_HANDLE
Dx12Device::Impl::dsvCpuHandle(uint32_t index) const {
  auto start = dsv_heap->GetCPUDescriptorHandleForHeapStart();
  start.ptr += static_cast<SIZE_T>(index) * dsv_increment;
  return start;
}

D3D12_CPU_DESCRIPTOR_HANDLE
Dx12Device::Impl::srvCpuHandle(uint32_t index) const {
  auto start = cbv_srv_uav_heap->GetCPUDescriptorHandleForHeapStart();
  start.ptr += static_cast<SIZE_T>(index) * cbv_srv_uav_increment;
  return start;
}

D3D12_GPU_DESCRIPTOR_HANDLE
Dx12Device::Impl::srvGpuHandle(uint32_t index) const {
  auto start = cbv_srv_uav_heap->GetGPUDescriptorHandleForHeapStart();
  start.ptr += static_cast<UINT64>(index) * cbv_srv_uav_increment;
  return start;
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_DX12
