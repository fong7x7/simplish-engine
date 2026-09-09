#include "dx12-command-list.h"
#include "dx12-device-impl.h"
#include "dx12-format-map.h"

#ifdef ENGINE_RENDERER_DX12

#include "dx12-builtin-pipelines.h"
#include "dx12-copy-alignment.h"
#include "dx12-texture-lookup.h"

#include <D3D12MemAlloc.h>
#include <array>
#include <climits>
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

  /// Channels in a capture, which is always RGBA8 by the time stb sees it.
  constexpr int CAPTURE_CHANNELS = 4;

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

  /// State a buffer on `heap` is created in and, for the CPU-visible heaps,
  /// stays in for its whole life.
  D3D12_RESOURCE_STATES initialStateForHeap(D3D12_HEAP_TYPE heap) {
    if (heap == D3D12_HEAP_TYPE_UPLOAD) {
      return D3D12_RESOURCE_STATE_GENERIC_READ;
    }
    return heap == D3D12_HEAP_TYPE_READBACK ? D3D12_RESOURCE_STATE_COPY_DEST
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

  /// A render or depth target created without one of these warns on every
  /// clear and clears more slowly besides.
  D3D12_CLEAR_VALUE buildClearValue(const RhiTextureDesc& desc) {
    D3D12_CLEAR_VALUE clear{};
    clear.Format = toDxgiFormat(desc.format);
    if (desc.usage & RhiTextureUsage::DEPTH_STENCIL) {
      clear.DepthStencil.Depth = 1.0f;
    }
    return clear;
  }

  /// Clear value pointer for a texture, or null when its usage forbids one.
  const D3D12_CLEAR_VALUE* clearValueFor(const RhiTextureDesc& desc,
                                         const D3D12_CLEAR_VALUE& storage) {
    const bool is_target = (desc.usage & RhiTextureUsage::RENDER_TARGET) ||
                           (desc.usage & RhiTextureUsage::DEPTH_STENCIL);
    return is_target ? &storage : nullptr;
  }

  /// Create a CPU-visible buffer on `heap` for one copy's worth of bytes.
  Dx12Buffer createStagingBuffer(D3D12MA::Allocator* alloc,
                                 D3D12_HEAP_TYPE heap, uint64_t size) {
    D3D12MA::ALLOCATION_DESC alloc_desc{};
    alloc_desc.HeapType = heap;
    auto buf_desc = buildBufferDesc(size);
    Dx12Buffer buf{};
    buf.host_visible = true;
    alloc->CreateResource(&alloc_desc, &buf_desc, initialStateForHeap(heap),
                          nullptr, &buf.allocation,
                          IID_PPV_ARGS(&buf.resource));
    return buf;
  }

  void releaseStagingBuffer(const Dx12Buffer& buf) {
    if (buf.resource != nullptr) {
      buf.resource->Release();
    }
    if (buf.allocation != nullptr) {
      buf.allocation->Release();
    }
  }

  // -----------------------------------------------------------------
  // Descriptor views
  // -----------------------------------------------------------------

  /// Create a texture SRV if the texture is sampled.
  void createSrvIfNeeded(Dx12Device::Impl& impl, Dx12Texture& tex,
                         RhiTextureUsage usage) {
    if (!(usage & RhiTextureUsage::SAMPLED)) {
      return;
    }
    tex.srv_index = impl.srv_allocator.allocate();
    if (tex.srv_index == DX12_DESCRIPTOR_INDEX_NONE) {
      return;  // Heap full: the texture still exists, it just never binds.
    }
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
    tex.rtv_index = impl.rtv_allocator.allocate();
    if (tex.rtv_index == DX12_DESCRIPTOR_INDEX_NONE) {
      return;
    }
    auto handle = impl.rtvCpuHandle(tex.rtv_index);
    impl.device->CreateRenderTargetView(tex.resource, nullptr, handle);
    tex.rtv_handle = handle;
  }

  /// Create a DSV for a depth/stencil texture.
  void createDsvIfNeeded(Dx12Device::Impl& impl, Dx12Texture& tex,
                         RhiTextureUsage usage) {
    if (!(usage & RhiTextureUsage::DEPTH_STENCIL)) {
      return;
    }
    tex.dsv_index = impl.dsv_allocator.allocate();
    if (tex.dsv_index == DX12_DESCRIPTOR_INDEX_NONE) {
      return;
    }
    auto handle = impl.dsvCpuHandle(tex.dsv_index);
    impl.device->CreateDepthStencilView(tex.resource, nullptr, handle);
    tex.dsv_handle = handle;
  }

  /// Hand every descriptor index a texture held back to its allocator.
  ///
  /// The RTV and DSV heaps have room for tens of views, and the editor
  /// recreates its depth target on every window resize — without this the
  /// DSV heap runs dry after a few dozen of them.
  void freeTextureDescriptors(Dx12Device::Impl& impl, const Dx12Texture& tex) {
    if (tex.srv_index != DX12_DESCRIPTOR_INDEX_NONE) {
      impl.srv_allocator.free(tex.srv_index);
    }
    if (tex.rtv_index != DX12_DESCRIPTOR_INDEX_NONE) {
      impl.rtv_allocator.free(tex.rtv_index);
    }
    if (tex.dsv_index != DX12_DESCRIPTOR_INDEX_NONE) {
      impl.dsv_allocator.free(tex.dsv_index);
    }
  }

  // -----------------------------------------------------------------
  // Texture upload
  // -----------------------------------------------------------------

  /// Where an upload's rows sit on each side of the copy.
  struct Dx12UploadLayout {
    /// Row pitch in the staging buffer; a multiple of 256.
    uint32_t dst_pitch = 0;
    /// Row pitch in the caller's pixels.
    uint32_t src_pitch = 0;
    /// Bytes of each row that carry texels.
    uint32_t row_bytes = 0;
  };

  /// The staging buffer of one upload plus the pitch its rows sit at.
  struct Dx12UploadSource {
    /// Upload-heap buffer holding the repitched rows.
    ID3D12Resource* resource = nullptr;
    /// Row pitch inside `resource`.
    uint32_t row_pitch = 0;
  };

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

  Dx12UploadLayout buildUploadLayout(const RhiTextureUpdate2D& u) {
    const uint32_t bpp = dx12BytesPerTexel(u.format);
    Dx12UploadLayout layout{};
    layout.row_bytes = u.width * bpp;
    layout.src_pitch =
        u.bytes_per_row != 0 ? u.bytes_per_row : layout.row_bytes;
    layout.dst_pitch = dx12AlignRowPitch(layout.row_bytes);
    return layout;
  }

  /// Copy `rows` rows across, repitching from source to staging.
  void repitchRows(void* dst, const void* src, const Dx12UploadLayout& layout,
                   uint32_t rows) {
    auto* out = static_cast<uint8_t*>(dst);
    const auto* in = static_cast<const uint8_t*>(src);
    for (uint32_t y = 0; y < rows; ++y) {
      std::memcpy(out + static_cast<size_t>(y) * layout.dst_pitch,
                  in + static_cast<size_t>(y) * layout.src_pitch,
                  layout.row_bytes);
    }
  }

  bool fillUploadStaging(const Dx12Buffer& staging,
                         const RhiTextureUpdate2D& update,
                         const Dx12UploadLayout& layout) {
    void* mapped = nullptr;
    if (FAILED(staging.resource->Map(0, nullptr, &mapped))) {
      return false;
    }
    repitchRows(mapped, update.pixels, layout, update.height);
    staging.resource->Unmap(0, nullptr);
    return true;
  }

  D3D12_TEXTURE_COPY_LOCATION
  buildUploadSrcLoc(const Dx12UploadSource& src, const RhiTextureUpdate2D& u,
                    DXGI_FORMAT format) {
    D3D12_TEXTURE_COPY_LOCATION loc{};
    loc.pResource = src.resource;
    loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    loc.PlacedFootprint.Footprint.Format = format;
    loc.PlacedFootprint.Footprint.Width = u.width;
    loc.PlacedFootprint.Footprint.Height = u.height;
    loc.PlacedFootprint.Footprint.Depth = 1;
    loc.PlacedFootprint.Footprint.RowPitch = src.row_pitch;
    return loc;
  }

  /// Record and run the staging-to-texture copy on the utility list.
  ///
  /// The utility list, not the current frame's: resetting a frame's list
  /// here would throw away whatever the caller was recording into it.
  bool recordTextureUpload(Dx12Device::Impl& impl, RhiTextureHandle handle,
                           const RhiTextureUpdate2D& update,
                           const Dx12UploadSource& src) {
    auto* tex = impl.textures.lookup(handle);
    if (tex == nullptr || !impl.beginUtilityCommands()) {
      return false;
    }
    auto* list = impl.utility.command_list;
    dx12TransitionTexture(list, impl, handle, D3D12_RESOURCE_STATE_COPY_DEST);
    auto src_loc = buildUploadSrcLoc(src, update, tex->format);
    D3D12_TEXTURE_COPY_LOCATION dst_loc{};
    dst_loc.pResource = tex->resource;
    dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    list->CopyTextureRegion(&dst_loc, update.offset_x, update.offset_y, 0,
                            &src_loc, nullptr);
    impl.submitUtilityCommands();
    return true;
  }

  bool runTextureUpload(Dx12Device::Impl& impl, RhiTextureHandle handle,
                        const RhiTextureUpdate2D& update,
                        const Dx12UploadLayout& layout) {
    const uint64_t bytes =
        static_cast<uint64_t>(layout.dst_pitch) * update.height;
    auto staging =
        createStagingBuffer(impl.allocator, D3D12_HEAP_TYPE_UPLOAD, bytes);
    if (staging.resource == nullptr) {
      return false;
    }
    const bool ok = fillUploadStaging(staging, update, layout) &&
                    recordTextureUpload(impl, handle, update,
                                        {staging.resource, layout.dst_pitch});
    releaseStagingBuffer(staging);
    return ok;
  }

  bool uploadTexture2D(Dx12Device::Impl& impl, RhiTextureHandle handle,
                       const RhiTextureUpdate2D& update) {
    auto* tex = impl.textures.lookup(handle);
    if (tex == nullptr || !isValidTextureUpdate(*tex, update)) {
      return false;
    }
    const auto layout = buildUploadLayout(update);
    if (layout.src_pitch < layout.row_bytes) {
      return false;
    }
    return runTextureUpload(impl, handle, update, layout);
  }

  /// Whole-surface update describing a texture's `initial_pixels`.
  RhiTextureUpdate2D fullSurfaceUpdate(const RhiTextureDesc& desc) {
    RhiTextureUpdate2D update{};
    update.pixels = desc.initial_pixels;
    update.width = desc.width;
    update.height = desc.height;
    update.format = desc.format;
    return update;
  }

  /// Give a freshly created texture its views, table slot and initial data.
  RhiTextureHandle finishTextureCreate(Dx12Device::Impl& impl, Dx12Texture& tex,
                                       const RhiTextureDesc& desc) {
    createSrvIfNeeded(impl, tex, desc.usage);
    createRtvIfNeeded(impl, tex, desc.usage);
    createDsvIfNeeded(impl, tex, desc.usage);
    const auto handle = impl.textures.insert(std::move(tex));
    if (desc.initial_pixels != nullptr) {
      uploadTexture2D(impl, handle, fullSurfaceUpdate(desc));
    }
    return handle;
  }

  // -----------------------------------------------------------------
  // Capture helpers
  // -----------------------------------------------------------------

  /// Shape of a texture readback: the tight image, and the padded rows the
  /// copy actually lands in.
  struct Dx12ReadbackLayout {
    /// Texture width in texels.
    uint32_t width = 0;
    /// Texture height in texels.
    uint32_t height = 0;
    /// Bytes of each row that carry texels.
    uint32_t row_bytes = 0;
    /// Row pitch in the readback buffer; a multiple of 256.
    uint32_t row_pitch = 0;
    /// Texture pixel format, which decides whether channels need swapping.
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
  };

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

  /// Swap the red and blue channels in place.
  ///
  /// PNG and JPEG both want RGBA, and a BGRA source reaches the encoder
  /// with the two exchanged. The swapchain here is RGBA, so this runs only
  /// for a capture of a BGRA texture the caller named.
  void swizzleBgraToRgba(std::vector<uint8_t>& pixels) {
    for (size_t i = 0; i + 3 < pixels.size(); i += CAPTURE_CHANNELS) {
      std::swap(pixels[i], pixels[i + 2]);
    }
  }

  bool isBgraFormat(DXGI_FORMAT format) {
    return format == DXGI_FORMAT_B8G8R8A8_UNORM ||
           format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
  }

  Dx12ReadbackLayout buildReadbackLayout(const D3D12_RESOURCE_DESC& desc) {
    Dx12ReadbackLayout layout{};
    layout.width = static_cast<uint32_t>(desc.Width);
    layout.height = desc.Height;
    layout.format = desc.Format;
    layout.row_bytes = layout.width * CAPTURE_CHANNELS;
    layout.row_pitch = dx12AlignRowPitch(layout.row_bytes);
    return layout;
  }

  D3D12_TEXTURE_COPY_LOCATION
  buildReadbackDstLoc(ID3D12Resource* staging,
                      const Dx12ReadbackLayout& layout) {
    D3D12_TEXTURE_COPY_LOCATION loc{};
    loc.pResource = staging;
    loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    loc.PlacedFootprint.Footprint.Format = layout.format;
    loc.PlacedFootprint.Footprint.Width = layout.width;
    loc.PlacedFootprint.Footprint.Height = layout.height;
    loc.PlacedFootprint.Footprint.Depth = 1;
    loc.PlacedFootprint.Footprint.RowPitch = layout.row_pitch;
    return loc;
  }

  /// Drop the padding D3D12 required, leaving a tight RGBA image.
  std::vector<uint8_t> unpackRows(const void* src,
                                  const Dx12ReadbackLayout& layout) {
    std::vector<uint8_t> pixels(static_cast<size_t>(layout.row_bytes) *
                                layout.height);
    const auto* in = static_cast<const uint8_t*>(src);
    for (uint32_t y = 0; y < layout.height; ++y) {
      std::memcpy(pixels.data() + static_cast<size_t>(y) * layout.row_bytes,
                  in + static_cast<size_t>(y) * layout.row_pitch,
                  layout.row_bytes);
    }
    return pixels;
  }

  /// Put the captured texture back where a later pass expects it.
  void restoreAfterReadback(ID3D12GraphicsCommandList* list,
                            Dx12Device::Impl& impl, RhiTextureHandle source) {
    dx12TransitionTexture(list, impl, source,
                          dx12IsSwapchainHandle(source)
                              ? D3D12_RESOURCE_STATE_PRESENT
                              : D3D12_RESOURCE_STATE_COMMON);
  }

  bool recordReadback(Dx12Device::Impl& impl, RhiTextureHandle source,
                      const Dx12ReadbackLayout& layout,
                      ID3D12Resource* staging) {
    if (!impl.beginUtilityCommands()) {
      return false;
    }
    auto* list = impl.utility.command_list;
    dx12TransitionTexture(list, impl, source, D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION src_loc{};
    src_loc.pResource = dx12TextureResource(impl, source);
    src_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    auto dst_loc = buildReadbackDstLoc(staging, layout);
    list->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, nullptr);
    restoreAfterReadback(list, impl, source);
    impl.submitUtilityCommands();
    return true;
  }

  std::optional<RhiCaptureResult>
  encodeReadback(const Dx12Buffer& staging, const Dx12ReadbackLayout& layout,
                 const RhiCaptureRequest& request) {
    void* mapped = nullptr;
    if (FAILED(staging.resource->Map(0, nullptr, &mapped))) {
      return std::nullopt;
    }
    auto pixels = unpackRows(mapped, layout);
    staging.resource->Unmap(0, nullptr);
    if (isBgraFormat(layout.format)) {
      swizzleBgraToRgba(pixels);
    }
    auto encoded =
        encodePixels(pixels.data(), layout.width, layout.height, request);
    if (encoded.empty()) {
      return std::nullopt;
    }
    return RhiCaptureResult{std::move(encoded), layout.width, layout.height};
  }

  Dx12Buffer createReadbackStaging(Dx12Device::Impl& impl,
                                   const Dx12ReadbackLayout& layout) {
    const uint64_t bytes =
        static_cast<uint64_t>(layout.row_pitch) * layout.height;
    return createStagingBuffer(impl.allocator, D3D12_HEAP_TYPE_READBACK, bytes);
  }

  /// Copy a texture back to the CPU and encode it. The whole surface, like
  /// the Metal backend: the request's sub-rectangle is not honoured here
  /// either, and the result reports the size actually captured.
  std::optional<RhiCaptureResult> runCapture(Dx12Device::Impl& impl,
                                             RhiTextureHandle source,
                                             const RhiCaptureRequest& request) {
    auto* resource = dx12TextureResource(impl, source);
    if (resource == nullptr) {
      return std::nullopt;
    }
    const auto layout = buildReadbackLayout(resource->GetDesc());
    auto staging = createReadbackStaging(impl, layout);
    if (staging.resource == nullptr) {
      return std::nullopt;
    }
    std::optional<RhiCaptureResult> result;
    if (recordReadback(impl, source, layout, staging.resource)) {
      result = encodeReadback(staging, layout, request);
    }
    releaseStagingBuffer(staging);
    return result;
  }

  // -----------------------------------------------------------------
  // Shader and pipeline helpers
  // -----------------------------------------------------------------

  bool isValidDxilBytecode(const RhiShaderDesc& desc) {
    return desc.bytecode != nullptr && desc.bytecode_size > 0;
  }

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

  void fillGraphicsPsoStates(D3D12_GRAPHICS_PIPELINE_STATE_DESC& pso,
                             const RhiGraphicsPipelineDesc& desc) {
    pso.BlendState = buildBlendDesc(desc.blend);
    pso.RasterizerState = buildRasterDesc(desc.raster);
    pso.DepthStencilState = buildDepthStencilDesc(desc.depth_stencil);
    pso.PrimitiveTopologyType = toDx12TopologyType(desc.topology);
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = toDxgiFormat(desc.color_format);
    pso.DSVFormat = toDxgiFormat(desc.depth_format);
    pso.SampleDesc.Count = 1;
    pso.SampleMask = UINT_MAX;
  }

  D3D12_GRAPHICS_PIPELINE_STATE_DESC
  buildGraphicsPsoDesc(Dx12Device::Impl& impl,
                       const Dx12GraphicsPipelineParams& p,
                       const std::vector<D3D12_INPUT_ELEMENT_DESC>& elems) {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = impl.graphics_root_signature;
    pso.VS = toShaderBytecode(*p.vs);
    pso.PS = toShaderBytecode(*p.fs);
    pso.InputLayout = {elems.data(), static_cast<UINT>(elems.size())};
    fillGraphicsPsoStates(pso, *p.desc);
    return pso;
  }

  RhiPipelineHandle buildGraphicsPso(Dx12Device::Impl& impl,
                                     const Dx12GraphicsPipelineParams& p) {
    auto elems = buildInputLayout(p.desc->vertex_layout);
    auto pso_desc = buildGraphicsPsoDesc(impl, p, elems);
    Dx12Pipeline pipeline{};
    pipeline.root_signature = impl.graphics_root_signature;
    pipeline.bind_point = Dx12PipelineType::GRAPHICS;
    pipeline.topology = toDx12Topology(p.desc->topology);
    pipeline.vertex_stride = p.desc->vertex_layout.stride;
    if (FAILED(impl.device->CreateGraphicsPipelineState(
            &pso_desc, IID_PPV_ARGS(&pipeline.pipeline_state)))) {
      return RHI_PIPELINE_INVALID;
    }
    return impl.pipelines.insert(std::move(pipeline));
  }

  RhiPipelineHandle buildComputePso(Dx12Device::Impl& impl,
                                    const Dx12Shader& cs) {
    D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc{};
    pso_desc.pRootSignature = impl.compute_root_signature;
    pso_desc.CS = toShaderBytecode(cs);
    Dx12Pipeline pipeline{};
    pipeline.root_signature = impl.compute_root_signature;
    pipeline.bind_point = Dx12PipelineType::COMPUTE;
    if (FAILED(impl.device->CreateComputePipelineState(
            &pso_desc, IID_PPV_ARGS(&pipeline.pipeline_state)))) {
      return RHI_PIPELINE_INVALID;
    }
    return impl.pipelines.insert(std::move(pipeline));
  }

  /// Register a built-in PSO with the stride its vertex buffers use.
  RhiPipelineHandle insertBuiltinPipeline(Dx12Device::Impl& impl,
                                          ID3D12PipelineState* state,
                                          uint32_t vertex_stride) {
    Dx12Pipeline pipeline{};
    pipeline.pipeline_state = state;
    pipeline.root_signature = impl.graphics_root_signature;
    pipeline.bind_point = Dx12PipelineType::GRAPHICS;
    pipeline.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    pipeline.vertex_stride = vertex_stride;
    return impl.pipelines.insert(std::move(pipeline));
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
  // The constructor is private so that create() is the only way in, which
  // puts make_unique out of reach; the pointer is owned before the
  // statement ends.
  auto device = std::unique_ptr<Dx12Device>(
      new Dx12Device());  // NOLINT(bare-new-delete) — private ctor, owned here
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
  const auto heap = toDx12HeapType(desc.host_visible);
  D3D12MA::ALLOCATION_DESC alloc_desc{};
  alloc_desc.HeapType = heap;
  auto buf_desc = buildBufferDesc(desc.size);

  Dx12Buffer buf{};
  buf.host_visible = desc.host_visible;
  HRESULT hr = impl_->allocator->CreateResource(
      &alloc_desc, &buf_desc, initialStateForHeap(heap), nullptr,
      &buf.allocation, IID_PPV_ARGS(&buf.resource));
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
  releaseStagingBuffer(*removed);
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
  if (buf == nullptr || !buf->host_visible) {
    return;  // A device-only buffer was never mapped in the first place.
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
  auto clear = buildClearValue(desc);

  Dx12Texture tex{};
  tex.width = desc.width;
  tex.height = desc.height;
  tex.format = toDxgiFormat(desc.format);
  tex.rhi_format = desc.format;
  tex.state = initialStateForTexture(desc.usage);
  HRESULT hr = impl_->allocator->CreateResource(
      &alloc_desc, &tex_desc, tex.state, clearValueFor(desc, clear),
      &tex.allocation, IID_PPV_ARGS(&tex.resource));
  if (FAILED(hr)) {
    return RHI_TEXTURE_INVALID;
  }
  return finishTextureCreate(*impl_, tex, desc);
}

void Dx12Device::destroyTexture(RhiTextureHandle handle) {
  auto removed = impl_->textures.remove(handle);
  if (!removed.has_value()) {
    return;
  }
  freeTextureDescriptors(*impl_, *removed);
  if (removed->resource != nullptr) {
    removed->resource->Release();
  }
  if (removed->allocation != nullptr) {
    removed->allocation->Release();
  }
}

bool Dx12Device::updateTexture2D(RhiTextureHandle handle,
                                 const RhiTextureUpdate2D& update) {
  return uploadTexture2D(*impl_, handle, update);
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
  // The root signature belongs to the device and outlives every pipeline.
}

bool Dx12Device::tryCreateGuiPipeline(RhiPipelineHandle& out_pipeline) {
  auto* state =
      createDx12GuiPipelineState(impl_->device, impl_->graphics_root_signature,
                                 impl_->swapchain_rtv_format);
  if (state == nullptr) {
    return false;
  }
  out_pipeline = insertBuiltinPipeline(*impl_, state, dx12GuiVertexStride());
  return true;
}

bool Dx12Device::tryCreateMeshPipeline(RhiPipelineHandle& out_pipeline) {
  auto* state =
      createDx12MeshPipelineState(impl_->device, impl_->graphics_root_signature,
                                  impl_->swapchain_rtv_format);
  if (state == nullptr) {
    return false;
  }
  out_pipeline = insertBuiltinPipeline(*impl_, state, dx12MeshVertexStride());
  return true;
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
  if (impl_->swapchain == nullptr) {
    return false;
  }
  if (impl_->resize_pending) {
    impl_->recreateSwapchain();
  }
  impl_->beginFrameRecording();
  return true;
}

void Dx12Device::endFrame() {}

void Dx12Device::submit(RhiCommandList& cmd) {
  auto& dcmd = static_cast<Dx12CommandList&>(cmd);
  std::array<ID3D12CommandList*, 1> lists{dcmd.nativeCommandList()};
  impl_->command_queue->ExecuteCommandLists(1, lists.data());
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
  const RhiTextureHandle source = request.target != RHI_TEXTURE_INVALID
                                      ? request.target
                                      : backbufferTexture();
  return runCapture(*impl_, source, request);
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
