#include "dx12-command-list.h"
#include "dx12-device-impl.h"
#include "dx12-format-map.h"

#ifdef ENGINE_RENDERER_DX12

#include <D3D12MemAlloc.h>
#include <cstdint>
#include <cstring>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <engine/render/rhi-capture-request.h>
#include <engine/render/rhi-capture-result.h>
#include <engine/render/rhi-command-list.h>
#include <engine/render/rhi-texture-update-2d.h>
#include <engine/render/rhi-types.h>
#include <fstream>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

// stb_image_write — PNG/JPEG encoding for capture API.
// Implementation lives in stb-image-write-impl.cpp.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wsign-conversion"
#include <stb_image_write.h>
#pragma clang diagnostic pop

namespace eng::render {

// ---------------------------------------------------------------------------
// Anonymous-namespace helpers for resource creation
// ---------------------------------------------------------------------------

namespace {

  D3D12_RESOURCE_DESC buildBufferDesc(uint64_t size) {
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return desc;
  }

  D3D12_RESOURCE_DESC buildTextureDesc(const RhiTextureDesc& desc) {
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width = desc.width;
    rd.Height = desc.height;
    rd.DepthOrArraySize = static_cast<UINT16>(desc.array_layers);
    rd.MipLevels = static_cast<UINT16>(desc.mip_levels);
    rd.Format = toDxgiFormat(desc.format);
    rd.SampleDesc.Count = 1;
    rd.Flags = toDx12ResourceFlags(desc.usage);
    return rd;
  }

  D3D12_RESOURCE_STATES initialStateForBuffer(bool host_visible) {
    return host_visible ? D3D12_RESOURCE_STATE_GENERIC_READ
                        : D3D12_RESOURCE_STATE_COMMON;
  }

  D3D12_RESOURCE_STATES initialStateForTexture(RhiTextureUsage usage) {
    if (usage & RhiTextureUsage::DEPTH_STENCIL) {
      return D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }
    if (usage & RhiTextureUsage::RENDER_TARGET) {
      return D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    return D3D12_RESOURCE_STATE_COMMON;
  }

  /// Create a texture SRV if the texture is sampled.
  void createSrvIfNeeded(Dx12Device::Impl& impl, Dx12Texture& tex,
                         RhiTextureUsage usage) {
    if (!(usage & RhiTextureUsage::SAMPLED)) {
      return;
    }
    tex.srv_index = impl.srv_allocator.allocate();
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = tex.format;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Texture2D.MipLevels = 1;
    impl.device->CreateShaderResourceView(tex.resource, &srv_desc,
                                          impl.srvCpuHandle(tex.srv_index));
  }

  /// Create an RTV for a render target texture.
  void createRtvIfNeeded(Dx12Device::Impl& impl, Dx12Texture& tex,
                         RhiTextureUsage usage) {
    if (!(usage & RhiTextureUsage::RENDER_TARGET)) {
      return;
    }
    uint32_t index = impl.rtv_allocator.allocate();
    auto handle = impl.rtvCpuHandle(index);
    impl.device->CreateRenderTargetView(tex.resource, nullptr, handle);
    tex.rtv_handle = handle;
  }

  /// Create a DSV for a depth/stencil texture.
  void createDsvIfNeeded(Dx12Device::Impl& impl, Dx12Texture& tex,
                         RhiTextureUsage usage) {
    if (!(usage & RhiTextureUsage::DEPTH_STENCIL)) {
      return;
    }
    uint32_t index = impl.dsv_allocator.allocate();
    auto handle = impl.dsvCpuHandle(index);
    impl.device->CreateDepthStencilView(tex.resource, nullptr, handle);
    tex.dsv_handle = handle;
  }

  // -----------------------------------------------------------------
  // Capture helpers
  // -----------------------------------------------------------------

  constexpr int CAPTURE_CHANNELS = 4;

  /// stb callback: append bytes to a vector.
  void stbWriteCallback(void* context, void* data, int size) {
    auto* out = static_cast<std::vector<uint8_t>*>(context);
    const auto* bytes = static_cast<const uint8_t*>(data);
    out->insert(out->end(), bytes, bytes + size);
  }

  /// Encode raw RGBA pixels to PNG or JPEG via stb_image_write.
  std::vector<uint8_t> encodePixels(const uint8_t* data, uint32_t w, uint32_t h,
                                    const RhiCaptureRequest& req) {
    auto stride = static_cast<int>(w) * CAPTURE_CHANNELS;
    std::vector<uint8_t> encoded;
    if (req.format == RhiCaptureFormat::JPEG) {
      stbi_write_jpg_to_func(stbWriteCallback, &encoded, static_cast<int>(w),
                             static_cast<int>(h), CAPTURE_CHANNELS, data,
                             static_cast<int>(req.jpeg_quality));
    } else {
      stbi_write_png_to_func(stbWriteCallback, &encoded, static_cast<int>(w),
                             static_cast<int>(h), CAPTURE_CHANNELS, data,
                             stride);
    }
    return encoded;
  }

  /// Create a readback buffer for pixel capture.
  // NOLINTNEXTLINE(readability-non-const-parameter) — COM non-const
  Dx12Buffer createReadbackBuffer(D3D12MA::Allocator* alloc, uint64_t size) {
    D3D12MA::ALLOCATION_DESC alloc_desc{};
    alloc_desc.HeapType = D3D12_HEAP_TYPE_READBACK;
    auto buf_desc = buildBufferDesc(size);

    Dx12Buffer buf{};
    buf.host_visible = true;
    alloc->CreateResource(&alloc_desc, &buf_desc,
                          D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                          &buf.allocation, IID_PPV_ARGS(&buf.resource));
    return buf;
  }

  // --- Shader validation helpers ---

  bool isValidDxilBytecode(const RhiShaderDesc& desc) {
    return desc.bytecode != nullptr && desc.bytecode_size > 0;
  }

  // --- Pipeline creation helpers ---

  /// Grouped parameters for graphics pipeline creation.
  struct Dx12GraphicsPipelineParams {
    /// Vertex shader bytecode.
    const Dx12Shader* vs = nullptr;
    /// Fragment shader bytecode.
    const Dx12Shader* fs = nullptr;
    /// Pipeline descriptor.
    const RhiGraphicsPipelineDesc* desc = nullptr;
  };

  D3D12_SHADER_BYTECODE toShaderBytecode(const Dx12Shader& shader) {
    return {shader.bytecode.data(), shader.bytecode.size()};
  }

  D3D12_BLEND_DESC buildBlendDesc(const RhiBlendState& blend) {
    D3D12_BLEND_DESC desc{};
    desc.RenderTarget[0].BlendEnable = blend.enabled ? TRUE : FALSE;
    desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    if (blend.enabled) {
      desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
      desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
      desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
      desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
      desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
      desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    }
    return desc;
  }

  D3D12_RASTERIZER_DESC buildRasterDesc(const RhiRasterState& raster) {
    D3D12_RASTERIZER_DESC desc{};
    desc.FillMode =
        raster.wireframe ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
    desc.CullMode =
        raster.cull_back ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_NONE;
    desc.FrontCounterClockwise = raster.front_ccw ? TRUE : FALSE;
    desc.DepthClipEnable = TRUE;
    return desc;
  }

  D3D12_DEPTH_STENCIL_DESC
  buildDepthStencilDesc(const RhiDepthStencilState& ds) {
    D3D12_DEPTH_STENCIL_DESC desc{};
    desc.DepthEnable = ds.depth_test ? TRUE : FALSE;
    desc.DepthWriteMask = ds.depth_write ? D3D12_DEPTH_WRITE_MASK_ALL
                                         : D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    return desc;
  }

  /// Build vertex input layout from RhiVertexLayout.
  std::vector<D3D12_INPUT_ELEMENT_DESC>
  buildInputLayout(const RhiVertexLayout& layout) {
    std::vector<D3D12_INPUT_ELEMENT_DESC> elems(layout.attribute_count);
    for (uint32_t i = 0; i < layout.attribute_count; ++i) {
      const auto& attr = layout.attributes[i];
      elems[i].SemanticName = "ATTR";
      elems[i].SemanticIndex = attr.location;
      elems[i].Format = toDxgiFormat(attr.format);
      elems[i].InputSlot = 0;
      elems[i].AlignedByteOffset = attr.offset;
      elems[i].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    }
    return elems;
  }

  ID3D12RootSignature* createEmptyRootSignature(ID3D12Device5* device) {
    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ID3DBlob* signature = nullptr;
    ID3DBlob* error = nullptr;
    if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                           &signature, &error))) {
      if (error != nullptr) {
        error->Release();
      }
      return nullptr;
    }
    ID3D12RootSignature* root_sig = nullptr;
    HRESULT hr = device->CreateRootSignature(0, signature->GetBufferPointer(),
                                             signature->GetBufferSize(),
                                             IID_PPV_ARGS(&root_sig));
    signature->Release();
    if (error != nullptr) {
      error->Release();
    }
    return SUCCEEDED(hr) ? root_sig : nullptr;
  }

  RhiPipelineHandle buildGraphicsPso(Dx12Device::Impl& impl,
                                     const Dx12GraphicsPipelineParams& p) {
    auto* root_sig = createEmptyRootSignature(impl.device);
    if (root_sig == nullptr) {
      return RHI_PIPELINE_INVALID;
    }
    auto input_layout = buildInputLayout(p.desc->vertex_layout);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
    pso_desc.pRootSignature = root_sig;
    pso_desc.VS = toShaderBytecode(*p.vs);
    pso_desc.PS = toShaderBytecode(*p.fs);
    pso_desc.BlendState = buildBlendDesc(p.desc->blend);
    pso_desc.RasterizerState = buildRasterDesc(p.desc->raster);
    pso_desc.DepthStencilState = buildDepthStencilDesc(p.desc->depth_stencil);
    pso_desc.InputLayout = {input_layout.data(),
                            static_cast<UINT>(input_layout.size())};
    pso_desc.PrimitiveTopologyType = toDx12TopologyType(p.desc->topology);
    pso_desc.NumRenderTargets = 1;
    pso_desc.RTVFormats[0] = toDxgiFormat(p.desc->color_format);
    pso_desc.DSVFormat = toDxgiFormat(p.desc->depth_format);
    pso_desc.SampleDesc.Count = 1;
    pso_desc.SampleMask = UINT_MAX;

    Dx12Pipeline pipeline{};
    pipeline.root_signature = root_sig;
    pipeline.bind_point = Dx12PipelineType::GRAPHICS;
    pipeline.topology = toDx12Topology(p.desc->topology);
    pipeline.vertex_stride = p.desc->vertex_layout.stride;
    HRESULT hr = impl.device->CreateGraphicsPipelineState(
        &pso_desc, IID_PPV_ARGS(&pipeline.pipeline_state));
    if (FAILED(hr)) {
      root_sig->Release();
      return RHI_PIPELINE_INVALID;
    }
    return impl.pipelines.insert(std::move(pipeline));
  }

  RhiPipelineHandle buildComputePso(Dx12Device::Impl& impl,
                                    const Dx12Shader& cs) {
    auto* root_sig = createEmptyRootSignature(impl.device);
    if (root_sig == nullptr) {
      return RHI_PIPELINE_INVALID;
    }
    D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc{};
    pso_desc.pRootSignature = root_sig;
    pso_desc.CS = toShaderBytecode(cs);

    Dx12Pipeline pipeline{};
    pipeline.root_signature = root_sig;
    pipeline.bind_point = Dx12PipelineType::COMPUTE;
    HRESULT hr = impl.device->CreateComputePipelineState(
        &pso_desc, IID_PPV_ARGS(&pipeline.pipeline_state));
    if (FAILED(hr)) {
      root_sig->Release();
      return RHI_PIPELINE_INVALID;
    }
    return impl.pipelines.insert(std::move(pipeline));
  }

  // --- Texture update helpers ---

  bool isValidTextureUpdate(const Dx12Texture& tex,
                            const RhiTextureUpdate2D& u) {
    if (u.pixels == nullptr || u.width == 0 || u.height == 0) {
      return false;
    }
    if (u.format == RhiFormat::UNDEFINED || u.format != tex.rhi_format) {
      return false;
    }
    if (dx12BytesPerTexel(u.format) == 0) {
      return false;
    }
    return (u.offset_x + u.width <= tex.width) &&
           (u.offset_y + u.height <= tex.height);
  }

  uint32_t resolveRowBytes(const RhiTextureUpdate2D& u, uint32_t bpp) {
    return u.bytes_per_row != 0 ? u.bytes_per_row : u.width * bpp;
  }

}  // namespace

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

Dx12Device::Dx12Device() : impl_(std::make_unique<Impl>()) {}

Dx12Device::~Dx12Device() {
  if (!impl_) {
    return;
  }
  waitIdle();
  impl_->teardown();
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::optional<std::unique_ptr<Dx12Device>>
Dx12Device::create(const RenderConfig& config) {
  auto device = std::unique_ptr<Dx12Device>(new Dx12Device());
  device->impl_->config = config;
  if (!device->impl_->initAll()) {
    return std::nullopt;
  }
  return device;
}

// ---------------------------------------------------------------------------
// Identification
// ---------------------------------------------------------------------------

RhiBackend Dx12Device::backend() const {
  return RhiBackend::D_X12;
}

const RhiDeviceCapabilities& Dx12Device::capabilities() const {
  return impl_->caps;
}

// ---------------------------------------------------------------------------
// Buffer lifecycle
// ---------------------------------------------------------------------------

RhiBufferHandle Dx12Device::createBuffer(const RhiBufferDesc& desc) {
  D3D12MA::ALLOCATION_DESC alloc_desc{};
  alloc_desc.HeapType = toDx12HeapType(desc.host_visible);
  auto buf_desc = buildBufferDesc(desc.size);
  auto initial_state = initialStateForBuffer(desc.host_visible);

  Dx12Buffer buf{};
  buf.host_visible = desc.host_visible;
  HRESULT hr = impl_->allocator->CreateResource(
      &alloc_desc, &buf_desc, initial_state, nullptr, &buf.allocation,
      IID_PPV_ARGS(&buf.resource));
  if (FAILED(hr)) {
    return RHI_BUFFER_INVALID;
  }
  return impl_->buffers.insert(std::move(buf));
}

void Dx12Device::destroyBuffer(RhiBufferHandle handle) {
  auto removed = impl_->buffers.remove(handle);
  if (!removed.has_value()) {
    return;
  }
  if (removed->resource != nullptr) {
    removed->resource->Release();
  }
  if (removed->allocation != nullptr) {
    removed->allocation->Release();
  }
}

void* Dx12Device::mapBuffer(RhiBufferHandle handle) {
  auto* buf = impl_->buffers.lookup(handle);
  if (buf == nullptr || !buf->host_visible) {
    return nullptr;
  }
  void* data = nullptr;
  buf->resource->Map(0, nullptr, &data);
  return data;
}

void Dx12Device::unmapBuffer(RhiBufferHandle handle) {
  auto* buf = impl_->buffers.lookup(handle);
  if (buf == nullptr) {
    return;
  }
  buf->resource->Unmap(0, nullptr);
}

// ---------------------------------------------------------------------------
// Texture lifecycle
// ---------------------------------------------------------------------------

RhiTextureHandle Dx12Device::createTexture(const RhiTextureDesc& desc) {
  D3D12MA::ALLOCATION_DESC alloc_desc{};
  alloc_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
  auto tex_desc = buildTextureDesc(desc);
  auto initial_state = initialStateForTexture(desc.usage);

  Dx12Texture tex{};
  tex.width = desc.width;
  tex.height = desc.height;
  tex.format = toDxgiFormat(desc.format);
  tex.rhi_format = desc.format;
  HRESULT hr = impl_->allocator->CreateResource(
      &alloc_desc, &tex_desc, initial_state, nullptr, &tex.allocation,
      IID_PPV_ARGS(&tex.resource));
  if (FAILED(hr)) {
    return RHI_TEXTURE_INVALID;
  }
  createSrvIfNeeded(*impl_, tex, desc.usage);
  createRtvIfNeeded(*impl_, tex, desc.usage);
  createDsvIfNeeded(*impl_, tex, desc.usage);
  return impl_->textures.insert(std::move(tex));
}

void Dx12Device::destroyTexture(RhiTextureHandle handle) {
  auto removed = impl_->textures.remove(handle);
  if (!removed.has_value()) {
    return;
  }
  if (removed->resource != nullptr) {
    removed->resource->Release();
  }
  if (removed->allocation != nullptr) {
    removed->allocation->Release();
  }
}

bool Dx12Device::updateTexture2D(RhiTextureHandle handle,
                                 const RhiTextureUpdate2D& update) {
  auto* tex = impl_->textures.lookup(handle);
  if (tex == nullptr || !isValidTextureUpdate(*tex, update)) {
    return false;
  }
  auto bpp = dx12BytesPerTexel(update.format);
  auto row_bytes = resolveRowBytes(update, bpp);
  uint64_t size = static_cast<uint64_t>(row_bytes) * update.height;

  auto staging = createReadbackBuffer(impl_->allocator, size);
  if (staging.resource == nullptr) {
    return false;
  }
  // Upload pixels to staging
  void* mapped = nullptr;
  staging.resource->Map(0, nullptr, &mapped);
  std::memcpy(mapped, update.pixels, size);
  staging.resource->Unmap(0, nullptr);

  // Record copy command
  auto& frame = impl_->frames[impl_->frame_index];
  frame.command_allocator->Reset();
  frame.command_list->Reset(frame.command_allocator, nullptr);

  D3D12_TEXTURE_COPY_LOCATION dst_loc{};
  dst_loc.pResource = tex->resource;
  dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  D3D12_TEXTURE_COPY_LOCATION src_loc{};
  src_loc.pResource = staging.resource;
  src_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  src_loc.PlacedFootprint.Footprint.Format = tex->format;
  src_loc.PlacedFootprint.Footprint.Width = update.width;
  src_loc.PlacedFootprint.Footprint.Height = update.height;
  src_loc.PlacedFootprint.Footprint.Depth = 1;
  src_loc.PlacedFootprint.Footprint.RowPitch = row_bytes;

  frame.command_list->CopyTextureRegion(&dst_loc, update.offset_x,
                                        update.offset_y, 0, &src_loc, nullptr);
  frame.command_list->Close();

  ID3D12CommandList* lists[] = {frame.command_list};
  impl_->command_queue->ExecuteCommandLists(1, lists);
  impl_->command_queue->Signal(frame.fence, ++frame.fence_value);
#ifdef _WIN32
  frame.fence->SetEventOnCompletion(frame.fence_value, frame.fence_event);
  WaitForSingleObject(frame.fence_event, INFINITE);
#endif
  staging.resource->Release();
  staging.allocation->Release();
  return true;
}

// ---------------------------------------------------------------------------
// Shader lifecycle
// ---------------------------------------------------------------------------

RhiShaderHandle Dx12Device::createShader(const RhiShaderDesc& desc) {
  if (!isValidDxilBytecode(desc)) {
    return RHI_SHADER_INVALID;
  }
  Dx12Shader shader{};
  shader.bytecode.assign(desc.bytecode, desc.bytecode + desc.bytecode_size);
  shader.stage = desc.stage;
  return impl_->shaders.insert(std::move(shader));
}

void Dx12Device::destroyShader(RhiShaderHandle handle) {
  impl_->shaders.remove(handle);
}

// ---------------------------------------------------------------------------
// Pipeline lifecycle
// ---------------------------------------------------------------------------

RhiPipelineHandle
Dx12Device::createGraphicsPipeline(const RhiGraphicsPipelineDesc& desc) {
  auto* vs = impl_->shaders.lookup(desc.vertex_shader);
  auto* fs = impl_->shaders.lookup(desc.fragment_shader);
  if (vs == nullptr || fs == nullptr) {
    return RHI_PIPELINE_INVALID;
  }
  return buildGraphicsPso(*impl_, {vs, fs, &desc});
}

RhiPipelineHandle
Dx12Device::createComputePipeline(const RhiComputePipelineDesc& desc) {
  auto* cs = impl_->shaders.lookup(desc.compute_shader);
  if (cs == nullptr) {
    return RHI_PIPELINE_INVALID;
  }
  return buildComputePso(*impl_, *cs);
}

void Dx12Device::destroyPipeline(RhiPipelineHandle handle) {
  auto removed = impl_->pipelines.remove(handle);
  if (!removed.has_value()) {
    return;
  }
  if (removed->pipeline_state != nullptr) {
    removed->pipeline_state->Release();
  }
  if (removed->root_signature != nullptr) {
    removed->root_signature->Release();
  }
}

// ---------------------------------------------------------------------------
// Swap chain accessors
// ---------------------------------------------------------------------------

RhiTextureHandle Dx12Device::backbufferTexture() const {
  return static_cast<RhiTextureHandle>(impl_->image_index + 1);
}

uint32_t Dx12Device::backbufferWidth() const {
  return impl_->swapchain_width;
}

uint32_t Dx12Device::backbufferHeight() const {
  return impl_->swapchain_height;
}

void Dx12Device::resizeSwapchain(uint32_t width, uint32_t height) {
  if (width == 0U || height == 0U) {
    return;
  }
  impl_->pending_width = width;
  impl_->pending_height = height;
  impl_->resize_pending = true;
}

// ---------------------------------------------------------------------------
// Frame management
// ---------------------------------------------------------------------------

bool Dx12Device::beginFrame() {
  if (impl_->resize_pending) {
    impl_->recreateSwapchain();
  }
  auto& frame = impl_->frames[impl_->frame_index];

  // Wait for this frame's GPU work to complete
  if (frame.fence->GetCompletedValue() < frame.fence_value) {
#ifdef _WIN32
    frame.fence->SetEventOnCompletion(frame.fence_value, frame.fence_event);
    WaitForSingleObject(frame.fence_event, INFINITE);
#endif
  }

  impl_->image_index = impl_->swapchain->GetCurrentBackBufferIndex();
  frame.command_allocator->Reset();
  frame.command_list->Reset(frame.command_allocator, nullptr);
  return true;
}

void Dx12Device::endFrame() {}

void Dx12Device::submit(RhiCommandList& cmd) {
  auto& dcmd = static_cast<Dx12CommandList&>(cmd);
  ID3D12CommandList* lists[] = {dcmd.nativeCommandList()};
  impl_->command_queue->ExecuteCommandLists(1, lists);
}

bool Dx12Device::present() {
  UINT sync_interval = impl_->config.vsync ? 1 : 0;
  HRESULT hr = impl_->swapchain->Present(sync_interval, 0);

  // Signal fence for this frame
  auto& frame = impl_->frames[impl_->frame_index];
  ++frame.fence_value;
  impl_->command_queue->Signal(frame.fence, frame.fence_value);

  impl_->frame_index = (impl_->frame_index + 1) % DX12_FRAMES_IN_FLIGHT;

  if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
    return false;
  }
  return SUCCEEDED(hr);
}

// ---------------------------------------------------------------------------
// Command list creation
// ---------------------------------------------------------------------------

std::unique_ptr<RhiCommandList> Dx12Device::createCommandList() {
  auto& frame = impl_->frames[impl_->frame_index];
  return std::make_unique<Dx12CommandList>(frame.command_list, *impl_);
}

// ---------------------------------------------------------------------------
// Capture
// ---------------------------------------------------------------------------

std::optional<RhiCaptureResult>
Dx12Device::captureFramebuffer(const RhiCaptureRequest& request) {
  waitIdle();
  auto w = request.width > 0 ? request.width : impl_->swapchain_width;
  auto h = request.height > 0 ? request.height : impl_->swapchain_height;
  uint64_t pixel_size = static_cast<uint64_t>(w) * h * CAPTURE_CHANNELS;
  auto staging = createReadbackBuffer(impl_->allocator, pixel_size);
  if (staging.resource == nullptr) {
    return std::nullopt;
  }
  // Record capture commands
  auto& frame = impl_->frames[0];
  frame.command_allocator->Reset();
  frame.command_list->Reset(frame.command_allocator, nullptr);

  auto* src = impl_->swapchain_images[impl_->image_index];

  // Barrier: PRESENT → COPY_SOURCE
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = src;
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  frame.command_list->ResourceBarrier(1, &barrier);

  // Copy texture region to readback buffer
  D3D12_TEXTURE_COPY_LOCATION dst_loc{};
  dst_loc.pResource = staging.resource;
  dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  dst_loc.PlacedFootprint.Footprint.Format = impl_->swapchain_format;
  dst_loc.PlacedFootprint.Footprint.Width = w;
  dst_loc.PlacedFootprint.Footprint.Height = h;
  dst_loc.PlacedFootprint.Footprint.Depth = 1;
  dst_loc.PlacedFootprint.Footprint.RowPitch =
      w * CAPTURE_CHANNELS;  // RGBA8 = 4 bytes
  D3D12_TEXTURE_COPY_LOCATION src_loc{};
  src_loc.pResource = src;
  src_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  frame.command_list->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, nullptr);

  // Barrier: COPY_SOURCE → PRESENT
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
  frame.command_list->ResourceBarrier(1, &barrier);

  frame.command_list->Close();
  ID3D12CommandList* lists[] = {frame.command_list};
  impl_->command_queue->ExecuteCommandLists(1, lists);
  impl_->command_queue->Signal(frame.fence, ++frame.fence_value);
#ifdef _WIN32
  frame.fence->SetEventOnCompletion(frame.fence_value, frame.fence_event);
  WaitForSingleObject(frame.fence_event, INFINITE);
#endif

  // Map and encode
  void* mapped = nullptr;
  staging.resource->Map(0, nullptr, &mapped);
  auto encoded =
      encodePixels(static_cast<const uint8_t*>(mapped), w, h, request);
  staging.resource->Unmap(0, nullptr);

  staging.resource->Release();
  staging.allocation->Release();

  if (encoded.empty()) {
    return std::nullopt;
  }
  return RhiCaptureResult{std::move(encoded), w, h};
}

bool Dx12Device::captureToFile(const RhiCaptureRequest& request,
                               std::string_view path) {
  auto result = captureFramebuffer(request);
  if (!result.has_value()) {
    return false;
  }
  std::ofstream file(std::string(path), std::ios::binary);
  if (!file.is_open()) {
    return false;
  }
  file.write(reinterpret_cast<const char*>(result->data.data()),
             static_cast<std::streamsize>(result->data.size()));
  return file.good();
}

// ---------------------------------------------------------------------------
// Extensions
// ---------------------------------------------------------------------------

IRhiRayTracing* Dx12Device::rayTracing() {
  return nullptr;
}

// ---------------------------------------------------------------------------
// Synchronization
// ---------------------------------------------------------------------------

void Dx12Device::waitIdle() {
  if (impl_->command_queue == nullptr) {
    return;
  }
  impl_->waitAllFrames();
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_DX12
