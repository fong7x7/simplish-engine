// Real Metal RHI device + command list implementation.
// Uses Apple Metal API via Objective-C++ for GPU resource management,
// frame presentation, command recording, and submission.
// Falls back to MetalStubDevice when native_window is nullptr.

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
// SDL headers use C-style casts internally; suppress warnings.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>
#pragma clang diagnostic pop

#include "metal-format-map.h"
#include "metal-resource-map.h"
#include "metal-stub-device.h"
#include "metal-type-converters.h"

#include <cstring>
#include <fstream>
#include <memory>
#include <optional>
#include <utility>

// stb_image_write — PNG/JPEG encoding for capture API.
// Implementation lives in stb-image-write-impl.cpp.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wsign-conversion"
#include <stb_image_write.h>
#pragma clang diagnostic pop
#include <engine/render/backends/metal/metal-rhi-command-list.h>
#include <engine/render/backends/metal/metal-rhi-device.h>
#include <engine/render/rhi-capture-request.h>
#include <engine/render/rhi-capture-result.h>
#include <engine/render/rhi-command-list.h>
#include <engine/render/rhi-compute-pipeline-desc.h>
#include <engine/render/rhi-device-capabilities.h>
#include <engine/render/rhi-device.h>
#include <engine/render/rhi-texture-update-2d.h>
#include <engine/render/rhi-shader-desc.h>
#include <engine/render/rhi-types.h>

namespace eng {
namespace {

  /// Entry for a compiled shader: library + function pair.
  struct ShaderEntry {
    /// Compiled metallib library.
    id<MTLLibrary> library = nil;
    /// Extracted entry point function.
    id<MTLFunction> function = nil;
  };

  /// Entry for a created pipeline state object.
  struct PipelineEntry {
    /// Render pipeline state (nil for compute pipelines).
    id<MTLRenderPipelineState> render_pso = nil;
    /// Compute pipeline state (nil for graphics pipelines).
    id<MTLComputePipelineState> compute_pso = nil;
    /// Depth/stencil state (nil for compute pipelines).
    id<MTLDepthStencilState> depth_stencil = nil;
    /// Topology stored for draw calls.
    RhiPrimitiveTopology topology = RhiPrimitiveTopology::TRIANGLE_LIST;
    /// Raster state for encoder configuration.
    RhiRasterState raster{};
  };

  /// Parameters for a blit copy from texture to staging buffer.
  struct RhiCaptureBlitParams {
    /// Source texture to copy from.
    id<MTLTexture> texture = nil;
    /// Destination staging buffer to copy into.
    id<MTLBuffer> buffer = nil;
    /// Width in pixels.
    uint32_t width = 0;
    /// Height in pixels.
    uint32_t height = 0;
  };

  /// Maximum descriptor sets supported by the Metal backend.
  constexpr uint32_t METAL_MAX_DESCRIPTOR_SETS = 31;

  /// First valid handle value (0 is invalid sentinel).
  constexpr uint64_t INITIAL_HANDLE = 1;

  /// Assumed bytes per pixel for BGRA8 backbuffer copies.
  constexpr uint32_t BACKBUFFER_BYTES_PER_PIXEL = 4;

  /// Maximum 2D texture dimension (typical Apple GPU limit).
  constexpr uint32_t MAX_TEXTURE_DIM_2D = 16384;

  /// Number of color channels in capture output.
  constexpr int CAPTURE_CHANNELS = 4;

  /// stb callback: append encoded bytes to a vector.
  void stbWriteCallback(void* context, void* data, int size) {
    auto* out = static_cast<std::vector<uint8_t>*>(context);
    const auto* bytes = static_cast<const uint8_t*>(data);
    out->insert(out->end(), bytes, bytes + size);
  }

  /// Encode raw BGRA pixels to PNG or JPEG via stb_image_write.
  std::vector<uint8_t> encodePixels(const uint8_t* data, uint32_t w,
                                    uint32_t h, RhiCaptureFormat fmt,
                                    uint8_t jpeg_quality) {
    auto stride = static_cast<int>(w) * CAPTURE_CHANNELS;
    std::vector<uint8_t> encoded;
    if (fmt == RhiCaptureFormat::JPEG) {
      stbi_write_jpg_to_func(stbWriteCallback, &encoded,
                             static_cast<int>(w), static_cast<int>(h),
                             CAPTURE_CHANNELS, data,
                             static_cast<int>(jpeg_quality));
    } else {
      stbi_write_png_to_func(stbWriteCallback, &encoded,
                             static_cast<int>(w), static_cast<int>(h),
                             CAPTURE_CHANNELS, data, stride);
    }
    return encoded;
  }

  // ----- Helper functions (defined before use) -----

  /// Convert RhiTextureUsage bitmask to MTLTextureUsage bitmask.
  MTLTextureUsage toMtlTextureUsage(RhiTextureUsage rhi_usage) {
    MTLTextureUsage usage = 0;
    if (rhi_usage & RhiTextureUsage::SAMPLED) {
      usage |= MTLTextureUsageShaderRead;
    }
    if (rhi_usage & RhiTextureUsage::STORAGE) {
      usage |= MTLTextureUsageShaderWrite;
    }
    if (rhi_usage & RhiTextureUsage::RENDER_TARGET ||
        rhi_usage & RhiTextureUsage::DEPTH_STENCIL) {
      usage |= MTLTextureUsageRenderTarget;
    }
    return usage;
  }

  /// Choose MTLResourceOptions based on host visibility and unified memory.
  MTLResourceOptions bufferStorageMode(bool host_visible,
                                       bool unified_memory) {
    if (host_visible && unified_memory) {
      return MTLResourceStorageModeShared;
    }
    if (host_visible) { return MTLResourceStorageModeManaged; }
    return MTLResourceStorageModePrivate;
  }

  /// Upload initial pixel data to a texture via replaceRegion.
  void uploadInitialPixels(id<MTLTexture> tex, const RhiTextureDesc& desc) {
    const auto bpp = bytesPerTexel(desc.format);
    const auto bytes_per_row = desc.width * bpp;
    MTLRegion region = MTLRegionMake2D(0, 0, desc.width, desc.height);
    [tex replaceRegion:region
           mipmapLevel:0
             withBytes:desc.initial_pixels
           bytesPerRow:bytes_per_row];
  }

  /// Swap the red and blue channels in place.
  ///
  /// The swapchain is BGRA8 and the blit copies its bytes untouched, while
  /// PNG and JPEG both want RGBA. Without this every capture comes back
  /// with red and blue exchanged.
  void swizzleBgraToRgba(std::vector<uint8_t>& pixels) {
    for (size_t i = 0; i + 3 < pixels.size(); i += CAPTURE_CHANNELS) {
      std::swap(pixels[i], pixels[i + 2]);
    }
  }

  /// Encode staging buffer contents to PNG or JPEG.
  RhiCaptureResult buildCaptureResult(id<MTLBuffer> staging,
                                      const RhiCaptureBlitParams& params,
                                      const RhiCaptureRequest& request) {
    const auto* raw = static_cast<const uint8_t*>([staging contents]);
    const size_t count = static_cast<size_t>(params.width) * params.height *
                         CAPTURE_CHANNELS;
    std::vector<uint8_t> pixels(raw, raw + count);
    swizzleBgraToRgba(pixels);
    auto encoded = encodePixels(pixels.data(), params.width, params.height,
                                request.format, request.jpeg_quality);
    RhiCaptureResult result;
    result.width = params.width;
    result.height = params.height;
    result.data = std::move(encoded);
    return result;
  }

  /// Build a MTLDepthStencilState from RhiDepthStencilState.
  id<MTLDepthStencilState>
  makeMtlDepthStencilState(id<MTLDevice> device,
                           const RhiDepthStencilState& state) {
    auto* dd = [[MTLDepthStencilDescriptor alloc] init];
    if (state.depth_test) {
      dd.depthCompareFunction = MTLCompareFunctionLess;
    } else {
      dd.depthCompareFunction = MTLCompareFunctionAlways;
    }
    dd.depthWriteEnabled = state.depth_write;
    return [device newDepthStencilStateWithDescriptor:dd];
  }

  /// Configure a single vertex attribute on the descriptor.
  void setVertexAttribute(MTLVertexDescriptor* vd,
                          const RhiVertexAttribute& attr) {
    vd.attributes[attr.location].format = toMtlVertexFormat(attr.format);
    vd.attributes[attr.location].offset = attr.offset;
    vd.attributes[attr.location].bufferIndex = 0;
  }

  /// Build a MTLVertexDescriptor from RhiVertexLayout.
  MTLVertexDescriptor*
  buildVertexDescriptor(const RhiVertexLayout& layout) {
    auto* vd = [[MTLVertexDescriptor alloc] init];
    for (uint32_t i = 0; i < layout.attribute_count; ++i) {
      setVertexAttribute(vd, layout.attributes[i]);
    }
    if (layout.stride > 0) {
      vd.layouts[0].stride = layout.stride;
      vd.layouts[0].stepRate = 1;
      vd.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    }
    return vd;
  }

  /// Encode a full-texture blit from texture to buffer via the given encoder.
  void encodeTextureToBufBlit(id<MTLBlitCommandEncoder> blit,
                              const RhiCaptureBlitParams& params) {
    auto row = params.width * BACKBUFFER_BYTES_PER_PIXEL;
    auto img = static_cast<size_t>(row) * params.height;
    [blit copyFromTexture:params.texture sourceSlice:0 sourceLevel:0
             sourceOrigin:MTLOriginMake(0, 0, 0)
               sourceSize:MTLSizeMake(params.width, params.height, 1)
                 toBuffer:params.buffer destinationOffset:0
       destinationBytesPerRow:row destinationBytesPerImage:img];
  }

  /// Apply alpha blending to the first color attachment.
  void applyAlphaBlend(MTLRenderPipelineColorAttachmentDescriptor* ca) {
    ca.blendingEnabled = YES;
    ca.sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    ca.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
  }

  /// Premultiplied-friendly “over” compositing for GUI (RGB + alpha factors).
  void applyGuiAlphaBlend(MTLRenderPipelineColorAttachmentDescriptor* ca) {
    ca.blendingEnabled = YES;
    ca.rgbBlendOperation = MTLBlendOperationAdd;
    ca.alphaBlendOperation = MTLBlendOperationAdd;
    ca.sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    ca.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    ca.sourceAlphaBlendFactor = MTLBlendFactorOne;
    ca.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
  }

  /// Build a MTLRenderPipelineDescriptor from shader entries and pipeline desc.
  MTLRenderPipelineDescriptor*
  buildPipelineDescriptor(const ShaderEntry& vs, const ShaderEntry& fs,
                          const RhiGraphicsPipelineDesc& desc) {
    auto* pd = [[MTLRenderPipelineDescriptor alloc] init];
    pd.vertexFunction = vs.function;
    pd.fragmentFunction = fs.function;
    pd.colorAttachments[0].pixelFormat = toMtlPixelFormat(desc.color_format);
    pd.depthAttachmentPixelFormat = toMtlPixelFormat(desc.depth_format);
    if (desc.blend.enabled) { applyAlphaBlend(pd.colorAttachments[0]); }
    if (desc.vertex_layout.attribute_count > 0) {
      pd.vertexDescriptor = buildVertexDescriptor(desc.vertex_layout);
    }
    return pd;
  }

  /// Byte stride for `GuiVertex` in the Metal GUI pipeline.
  constexpr NSUInteger GUI_VERTEX_STRIDE = 40;

  /// MSL source for screen-space GUI quads (matches `GuiVertex`).
  constexpr const char GUI_MSL_SOURCE[] = R"msl(
#include <metal_stdlib>
using namespace metal;

struct ScreenToNdc {
  float2 scale;
};

struct GuiVertexIn {
  float2 position [[attribute(0)]];
  float2 uv [[attribute(1)]];
  uint color [[attribute(2)]];
  float corner_radius [[attribute(3)]];
  float border_width [[attribute(4)]];
  uint flags [[attribute(5)]];
  float2 rect_wh [[attribute(6)]];
};

struct GuiVsOut {
  float4 position [[position]];
  float2 uv;
  float4 color;
  uint flags;
  float corner_radius;
  float border_width;
  float2 rect_wh;
};

/// sRGB-encoded byte (0–1) → linear for fragment output to an sRGB color attachment.
float srgb_byte_to_linear(float srgb) {
  if (srgb <= 0.04045f) {
    return srgb / 12.92f;
  }
  return pow((srgb + 0.055f) / 1.055f, 2.4f);
}

float4 unpack_rgba8888(uint c) {
  float r = float((c >> 0u) & 255u) / 255.0f;
  float g = float((c >> 8u) & 255u) / 255.0f;
  float b = float((c >> 16u) & 255u) / 255.0f;
  float a = float((c >> 24u) & 255u) / 255.0f;
  return float4(srgb_byte_to_linear(r), srgb_byte_to_linear(g),
                srgb_byte_to_linear(b), a);
}

vertex GuiVsOut gui_vs_main(GuiVertexIn in [[stage_in]],
                            constant ScreenToNdc& screen [[buffer(1)]]) {
  GuiVsOut out;
  out.position =
      float4(in.position.x * screen.scale.x - 1.0f,
             1.0f - in.position.y * screen.scale.y, 0.0f, 1.0f);
  out.uv = in.uv;
  out.color = unpack_rgba8888(in.color);
  out.flags = in.flags;
  out.corner_radius = in.corner_radius;
  out.border_width = in.border_width;
  out.rect_wh = in.rect_wh;
  return out;
}

float gui_rounded_shape_cover_from_p(float2 p, float2 rect_wh, float corner_r) {
  float rw = rect_wh.x;
  float rh = rect_wh.y;
  if (rw <= 0.f || rh <= 0.f) {
    return 0.f;
  }
  float r = min(max(corner_r, 0.f), min(rw, rh) * 0.5f);
  float2 half_ext = float2(rw, rh) * 0.5f;
  float2 b = max(half_ext - float2(r), float2(0));
  float2 q = abs(p) - b;
  float d = length(max(q, float2(0))) + min(max(q.x, q.y), 0.f) - r;
  float w = max(fwidth(d), 1e-4f);
  return 1.f - smoothstep(-w, w, d);
}

fragment float4 gui_fs_main(GuiVsOut in [[stage_in]],
                            texture2d<float> tex [[texture(0)]]) {
  constexpr sampler smp(filter::linear, address::clamp_to_edge);
  if ((in.flags & 2u) != 0u) {
    return tex.sample(smp, in.uv) * in.color;
  }
  float2 p = float2((in.uv.x - 0.5f) * in.rect_wh.x,
                    (in.uv.y - 0.5f) * in.rect_wh.y);
  float4 c = in.color;
  const float bw = in.border_width;
  const uint rounded_flag = in.flags & 4u;
  if (bw > 1e-5f) {
    float cr_o = (rounded_flag != 0u) ? in.corner_radius : 0.f;
    float outer_c = gui_rounded_shape_cover_from_p(p, in.rect_wh, cr_o);
    float irw = max(in.rect_wh.x - 2.f * bw, 0.f);
    float irh = max(in.rect_wh.y - 2.f * bw, 0.f);
    float in_r = (rounded_flag != 0u) ? max(in.corner_radius - bw, 0.f) : 0.f;
    float inner_c =
        gui_rounded_shape_cover_from_p(p, float2(irw, irh), in_r);
    float ring = outer_c * (1.f - inner_c);
    c.a *= ring;
    return c;
  }
  if (rounded_flag != 0u) {
    float cover = gui_rounded_shape_cover_from_p(p, in.rect_wh,
                                                 in.corner_radius);
    c.a *= cover;
    return c;
  }
  return c;
}
)msl";

  void setGuiVertexBufferLayout(MTLVertexDescriptor* vd) {
    vd.layouts[0].stride = GUI_VERTEX_STRIDE;
    vd.layouts[0].stepRate = 1;
    vd.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
  }

  void setGuiVertexAttrs01(MTLVertexDescriptor* vd) {
    vd.attributes[0].format = MTLVertexFormatFloat2;
    vd.attributes[0].offset = 0;
    vd.attributes[0].bufferIndex = 0;
    vd.attributes[1].format = MTLVertexFormatFloat2;
    vd.attributes[1].offset = 8;
    vd.attributes[1].bufferIndex = 0;
  }

  void setGuiVertexAttrs2345(MTLVertexDescriptor* vd) {
    vd.attributes[2].format = MTLVertexFormatUInt;
    vd.attributes[2].offset = 16;
    vd.attributes[2].bufferIndex = 0;
    vd.attributes[3].format = MTLVertexFormatFloat;
    vd.attributes[3].offset = 20;
    vd.attributes[3].bufferIndex = 0;
    vd.attributes[4].format = MTLVertexFormatFloat;
    vd.attributes[4].offset = 24;
    vd.attributes[4].bufferIndex = 0;
    vd.attributes[5].format = MTLVertexFormatUInt;
    vd.attributes[5].offset = 28;
    vd.attributes[5].bufferIndex = 0;
  }

  void setGuiVertexAttr6RectWh(MTLVertexDescriptor* vd) {
    vd.attributes[6].format = MTLVertexFormatFloat2;
    vd.attributes[6].offset = 32;
    vd.attributes[6].bufferIndex = 0;
  }

  MTLVertexDescriptor* makeGuiVertexDescriptor() {
    auto* vd = [[MTLVertexDescriptor alloc] init];
    setGuiVertexBufferLayout(vd);
    setGuiVertexAttrs01(vd);
    setGuiVertexAttrs2345(vd);
    setGuiVertexAttr6RectWh(vd);
    return vd;
  }

  /// Byte stride for `MeshVertex`: two tightly packed float3s.
  constexpr NSUInteger MESH_VERTEX_STRIDE = 24;

  /// MSL source for static meshes (matches `eng::MeshVertex`).
  ///
  /// The vertex stage takes the world-to-clip matrix as vertex stage bytes
  /// at slot 1, so a draw needs no descriptor set. Shading is one hard-coded
  /// directional light plus ambient: enough to read a model's form in the
  /// editor, and nothing the real lighting model has to agree with.
  constexpr const char MESH_MSL_SOURCE[] = R"msl(
#include <metal_stdlib>
using namespace metal;

struct MeshUniforms {
  float4x4 view_projection;
};

struct MeshVertexIn {
  float3 position [[attribute(0)]];
  float3 normal [[attribute(1)]];
};

struct MeshVsOut {
  float4 position [[position]];
  float3 normal;
};

/// Linear value for an sRGB colour component, for output to an sRGB target.
float mesh_srgb_to_linear(float srgb) {
  if (srgb <= 0.04045f) {
    return srgb / 12.92f;
  }
  return pow((srgb + 0.055f) / 1.055f, 2.4f);
}

vertex MeshVsOut mesh_vs_main(MeshVertexIn in [[stage_in]],
                              constant MeshUniforms& u [[buffer(1)]]) {
  MeshVsOut out;
  out.position = u.view_projection * float4(in.position, 1.0f);
  out.normal = in.normal;
  return out;
}

fragment float4 mesh_fs_main(MeshVsOut in [[stage_in]]) {
  float3 n = normalize(in.normal);
  // Up, and over the viewer's left shoulder: the direction the editor's
  // camera implies, so a model's top face reads brightest.
  float3 light = normalize(float3(-0.35f, -0.45f, 0.82f));
  float lambert = saturate(dot(n, light));
  float shade = 0.38f + 0.62f * lambert;
  float3 base = float3(0.74f, 0.76f, 0.80f) * shade;
  return float4(mesh_srgb_to_linear(base.r), mesh_srgb_to_linear(base.g),
                mesh_srgb_to_linear(base.b), 1.0f);
}
)msl";

  MTLVertexDescriptor* makeMeshVertexDescriptor() {
    auto* vd = [[MTLVertexDescriptor alloc] init];
    vd.layouts[0].stride = MESH_VERTEX_STRIDE;
    vd.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    vd.attributes[0].format = MTLVertexFormatFloat3;
    vd.attributes[0].offset = 0;
    vd.attributes[0].bufferIndex = 0;
    vd.attributes[1].format = MTLVertexFormatFloat3;
    vd.attributes[1].offset = 12;
    vd.attributes[1].bufferIndex = 0;
    return vd;
  }

  bool compileMeshShaderLibrary(id<MTLDevice> mtl_device,
                                id<MTLLibrary>* out_lib) {
    NSError* err = nil;
    NSString* src = [NSString stringWithUTF8String:MESH_MSL_SOURCE];
    *out_lib = [mtl_device newLibraryWithSource:src options:nil error:&err];
    if (*out_lib == nil) {
      (void)err;
      return false;
    }
    return true;
  }

  void configureMeshRenderPipelineDesc(MTLRenderPipelineDescriptor* pd,
                                       id<MTLFunction> vs,
                                       id<MTLFunction> fs) {
    pd.vertexFunction = vs;
    pd.fragmentFunction = fs;
    pd.vertexDescriptor = makeMeshVertexDescriptor();
    pd.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
    // Opaque geometry: no blending, and a real depth attachment.
    pd.colorAttachments[0].blendingEnabled = NO;
    pd.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
  }

  bool buildMeshPipelinePso(id<MTLDevice> mtl_device,
                            id<MTLRenderPipelineState>* out_pso) {
    id<MTLLibrary> lib = nil;
    if (!compileMeshShaderLibrary(mtl_device, &lib)) {
      return false;
    }
    id<MTLFunction> vs = [lib newFunctionWithName:@"mesh_vs_main"];
    id<MTLFunction> fs = [lib newFunctionWithName:@"mesh_fs_main"];
    if (vs == nil || fs == nil) {
      return false;
    }
    auto* pd = [[MTLRenderPipelineDescriptor alloc] init];
    configureMeshRenderPipelineDesc(pd, vs, fs);
    NSError* err = nil;
    *out_pso = [mtl_device newRenderPipelineStateWithDescriptor:pd error:&err];
    (void)err;
    return *out_pso != nil;
  }

  bool compileGuiShaderLibrary(id<MTLDevice> mtl_device,
                               id<MTLLibrary>* out_lib) {
    NSError* err = nil;
    NSString* src = [NSString stringWithUTF8String:GUI_MSL_SOURCE];
    *out_lib = [mtl_device newLibraryWithSource:src options:nil error:&err];
    if (*out_lib == nil) {
      (void)err;
      return false;
    }
    return true;
  }

  bool extractGuiShaderFunctions(id<MTLLibrary> lib, id<MTLFunction>* vs_out,
                                 id<MTLFunction>* fs_out) {
    *vs_out = [lib newFunctionWithName:@"gui_vs_main"];
    *fs_out = [lib newFunctionWithName:@"gui_fs_main"];
    return *vs_out != nil && *fs_out != nil;
  }

  void configureGuiRenderPipelineDesc(MTLRenderPipelineDescriptor* pd,
                                      id<MTLFunction> vs, id<MTLFunction> fs) {
    pd.vertexFunction = vs;
    pd.fragmentFunction = fs;
    pd.vertexDescriptor = makeGuiVertexDescriptor();
    pd.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
    pd.depthAttachmentPixelFormat = MTLPixelFormatInvalid;
    applyGuiAlphaBlend(pd.colorAttachments[0]);
  }

  bool createGuiRenderPipelineState(
      id<MTLDevice> mtl_device, id<MTLFunction> vs, id<MTLFunction> fs,
      id<MTLRenderPipelineState>* out_pso) {
    auto* pd = [[MTLRenderPipelineDescriptor alloc] init];
    configureGuiRenderPipelineDesc(pd, vs, fs);
    NSError* err = nil;
    *out_pso = [mtl_device newRenderPipelineStateWithDescriptor:pd error:&err];
    if (*out_pso == nil) {
      (void)err;
      return false;
    }
    return true;
  }

  bool buildGuiPipelinePso(id<MTLDevice> mtl_device,
                           id<MTLRenderPipelineState>* out_pso) {
    id<MTLLibrary> lib = nil;
    if (!compileGuiShaderLibrary(mtl_device, &lib)) {
      return false;
    }
    id<MTLFunction> vs = nil;
    id<MTLFunction> fs = nil;
    if (!extractGuiShaderFunctions(lib, &vs, &fs)) {
      return false;
    }
    return createGuiRenderPipelineState(mtl_device, vs, fs, out_pso);
  }

}  // namespace

// =========================================================================
// MetalRealDevice — class declaration
// =========================================================================

/// Real Metal RHI device backed by MTLDevice and CAMetalLayer.
/// All methods are main-thread-only.
class MetalRealDevice final : public RhiDevice {
public:
  static std::optional<std::unique_ptr<RhiDevice>>
  tryCreate(const MetalRhiConfig& config);

  ~MetalRealDevice() override;

  /// Initialise GPU objects after MTLDevice is obtained.
  bool initGpuResources(id<MTLDevice> device);

  RhiBackend backend() const override { return RhiBackend::METAL; }
  const RhiDeviceCapabilities& capabilities() const override { return caps_; }

  RhiBufferHandle createBuffer(const RhiBufferDesc& desc) override;
  void destroyBuffer(RhiBufferHandle handle) override;
  void* mapBuffer(RhiBufferHandle handle) override;
  void unmapBuffer(RhiBufferHandle handle) override;
  RhiTextureHandle createTexture(const RhiTextureDesc& desc) override;
  void destroyTexture(RhiTextureHandle handle) override;
  bool updateTexture2D(RhiTextureHandle handle,
                       const RhiTextureUpdate2D& update) override;

  RhiShaderHandle createShader(const RhiShaderDesc& desc) override;
  void destroyShader(RhiShaderHandle handle) override;
  RhiPipelineHandle
  createGraphicsPipeline(const RhiGraphicsPipelineDesc& desc) override;
  RhiPipelineHandle
  createComputePipeline(const RhiComputePipelineDesc& desc) override;
  void destroyPipeline(RhiPipelineHandle handle) override;

  bool tryCreateGuiPipeline(RhiPipelineHandle& out) override;
  bool tryCreateMeshPipeline(RhiPipelineHandle& out) override;

  RhiTextureHandle backbufferTexture() const override;
  uint32_t backbufferWidth() const override;
  uint32_t backbufferHeight() const override;
  void resizeSwapchain(uint32_t width, uint32_t height) override;

  bool beginFrame() override;
  void endFrame() override;
  void submit(RhiCommandList& cmd) override;
  bool present() override;
  std::unique_ptr<RhiCommandList> createCommandList() override;

  std::optional<RhiCaptureResult>
  captureFramebuffer(const RhiCaptureRequest& request) override;
  bool captureToFile(const RhiCaptureRequest&, std::string_view) override;
  IRhiRayTracing* rayTracing() override { return nullptr; }
  void waitIdle() override;

  /// Look up a buffer by handle. Returns nil if not found.
  id<MTLBuffer> lookupBuffer(RhiBufferHandle h) const;
  /// Look up a texture by handle. Returns nil if not found.
  id<MTLTexture> lookupTexture(RhiTextureHandle h) const;
  /// Look up a pipeline entry by handle.
  PipelineEntry lookupPipeline(RhiPipelineHandle h) const;

private:
  explicit MetalRealDevice(const MetalRhiConfig& config);
  void populateCapabilities();
  void configureLayer(id<MTLDevice> device);
  MTLTextureDescriptor* buildTextureDescriptor(const RhiTextureDesc& desc);
  ShaderEntry compileShader(const RhiShaderDesc& desc);
  void blitTextureToBuffer(const RhiCaptureBlitParams& params);
  id<MTLTexture> resolveCaptureTex(const RhiCaptureRequest& request);
  bool insertGuiPipelineFromPso(id<MTLRenderPipelineState> pso,
                                RhiPipelineHandle& out);
  bool insertMeshPipelineFromPso(id<MTLRenderPipelineState> pso,
                                 RhiPipelineHandle& out);

  /// Metal GPU device.
  id<MTLDevice> device_ = nil;
  /// Command submission queue.
  id<MTLCommandQueue> queue_ = nil;
  /// Presentation layer attached to the window.
  CAMetalLayer* layer_ = nil;
  /// SDL Metal view (owns the CAMetalLayer).
  SDL_MetalView metal_view_ = nullptr;
  /// Current frame's drawable.
  id<CAMetalDrawable> drawable_ = nil;
  /// Last submitted command buffer.
  id<MTLCommandBuffer> last_cmd_buffer_ = nil;
  /// Frame-in-flight semaphore for pacing.
  dispatch_semaphore_t frame_sem_ = nullptr;
  /// Startup configuration.
  MetalRhiConfig config_;
  /// Cached device capabilities.
  RhiDeviceCapabilities caps_{};
  /// Whether the GPU has unified CPU/GPU memory.
  bool unified_memory_ = false;
  /// Monotonic handle counter.
  uint64_t next_handle_ = INITIAL_HANDLE;
  /// Backbuffer handle (updated each frame from drawable).
  RhiTextureHandle backbuffer_handle_ = RHI_TEXTURE_INVALID;
  /// Current drawable texture.
  id<MTLTexture> backbuffer_texture_ = nil;
  /// Resource maps.
  MetalResourceMap<id<MTLBuffer>> buffers_;
  MetalResourceMap<id<MTLTexture>> textures_;
  MetalResourceMap<ShaderEntry> shaders_;
  MetalResourceMap<PipelineEntry> pipelines_;
  MetalResourceMap<bool> buffer_host_visible_;
};

// =========================================================================
// MetalRealCommandList — class declaration + implementation
// =========================================================================

namespace {

  /// Real Metal command list backed by MTLCommandBuffer and encoders.
  /// All methods are main-thread-only.
  class MetalRealCommandList final : public RhiCommandList {
  public:
    MetalRealCommandList(id<MTLCommandBuffer> cb, MetalRealDevice* dev)
      : cmd_buffer_(cb), device_(dev) {}

    void begin() override {}
    void end() override { endActiveEncoder(); }

    void beginRenderPass(const RhiRenderPassBeginInfo& info) override {
      @autoreleasepool {
        auto* rpd = [[MTLRenderPassDescriptor alloc] init];
        configureColorAttachments(rpd, info);
        configureDepthAttachment(rpd, info);
        render_enc_ = [cmd_buffer_ renderCommandEncoderWithDescriptor:rpd];
      }
    }

    void endRenderPass() override {
      if (render_enc_ != nil) {
        [render_enc_ endEncoding];
        render_enc_ = nil;
      }
    }

    void bindPipeline(RhiPipelineHandle pipeline) override {
      bound_ = device_->lookupPipeline(pipeline);
      applyBoundPipeline();
    }

    void bindVertexBuffer(RhiBufferHandle buf, uint64_t offset) override {
      id<MTLBuffer> b = device_->lookupBuffer(buf);
      if (render_enc_ != nil && b != nil) {
        [render_enc_ setVertexBuffer:b offset:offset atIndex:0];
      }
    }

    void bindIndexBuffer(RhiBufferHandle buf, uint64_t offset,
                         RhiIndexType idx_type) override {
      idx_buf_ = device_->lookupBuffer(buf);
      idx_off_ = offset;
      idx_type_ = toMtlIndexType(idx_type);
    }

    void bindDescriptorSet(uint32_t, RhiDescriptorSetHandle) override {}

    void setVertexStageBytes(const void* data, size_t size,
                             uint32_t slot) override {
      if (render_enc_ == nil || data == nullptr || size == 0) {
        return;
      }
      [render_enc_ setVertexBytes:data length:size atIndex:slot];
    }

    void bindFragmentTexture(RhiTextureHandle tex, uint32_t slot) override {
      if (render_enc_ == nil) {
        return;
      }
      [render_enc_ setFragmentTexture:device_->lookupTexture(tex)
                              atIndex:slot];
    }

    void setViewport(const RhiViewport& vp) override {
      if (render_enc_ == nil) { return; }
      MTLViewport m{vp.x, vp.y, vp.width, vp.height, vp.min_depth, vp.max_depth};
      [render_enc_ setViewport:m];
    }

    void setScissor(const RhiScissor& sc) override {
      if (render_enc_ == nil) { return; }
      MTLScissorRect r{static_cast<NSUInteger>(sc.x),
                       static_cast<NSUInteger>(sc.y), sc.width, sc.height};
      [render_enc_ setScissorRect:r];
    }

    void draw(const RhiDrawParams& p) override {
      if (render_enc_ == nil || bound_.render_pso == nil) { return; }
      [render_enc_ drawPrimitives:toMtlPrimitiveType(bound_.topology)
                      vertexStart:p.first_vertex
                      vertexCount:p.vertex_count
                    instanceCount:p.instance_count
                     baseInstance:p.first_instance];
    }

    void drawIndexed(const RhiDrawIndexedParams& p) override {
      if (render_enc_ == nil || idx_buf_ == nil || bound_.render_pso == nil) {
        return;
      }
      [render_enc_
          drawIndexedPrimitives:toMtlPrimitiveType(bound_.topology)
                     indexCount:p.index_count
                      indexType:idx_type_
                    indexBuffer:idx_buf_
              indexBufferOffset:idx_off_
                  instanceCount:p.instance_count
                     baseVertex:p.vertex_offset
                   baseInstance:p.first_instance];
    }

    void dispatch(uint32_t gx, uint32_t gy, uint32_t gz) override {
      @autoreleasepool {
        id<MTLComputeCommandEncoder> enc = [cmd_buffer_ computeCommandEncoder];
        if (bound_.compute_pso != nil) {
          [enc setComputePipelineState:bound_.compute_pso];
        }
        [enc dispatchThreadgroups:MTLSizeMake(gx, gy, gz)
            threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
        [enc endEncoding];
      }
    }

    void copyBuffer(const RhiCopyBufferParams& p) override {
      @autoreleasepool {
        id<MTLBuffer> src = device_->lookupBuffer(p.src);
        id<MTLBuffer> dst = device_->lookupBuffer(p.dst);
        if (src == nil || dst == nil) { return; }
        id<MTLBlitCommandEncoder> blit = [cmd_buffer_ blitCommandEncoder];
        [blit copyFromBuffer:src sourceOffset:p.src_offset
                    toBuffer:dst destinationOffset:p.dst_offset size:p.size];
        [blit endEncoding];
      }
    }

    void copyTextureToBuffer(RhiTextureHandle src_h,
                             RhiBufferHandle dst_h) override {
      @autoreleasepool {
        id<MTLTexture> tex = device_->lookupTexture(src_h);
        id<MTLBuffer> buf = device_->lookupBuffer(dst_h);
        if (tex == nil || buf == nil) { return; }
        auto w = static_cast<uint32_t>([tex width]);
        auto h = static_cast<uint32_t>([tex height]);
        id<MTLBlitCommandEncoder> blit = [cmd_buffer_ blitCommandEncoder];
        encodeTextureToBufBlit(blit, {tex, buf, w, h});
        [blit endEncoding];
      }
    }

    void textureBarrier(RhiTextureHandle, RhiTextureLayout,
                        RhiTextureLayout) override {
      // Metal handles most resource hazards automatically.
    }

  private:
    void endActiveEncoder() {
      if (render_enc_ != nil) {
        [render_enc_ endEncoding];
        render_enc_ = nil;
      }
    }

    void configureColorAttachments(MTLRenderPassDescriptor* rpd,
                                   const RhiRenderPassBeginInfo& info) {
      for (uint32_t i = 0; i < info.color_target_count; ++i) {
        auto* ca = rpd.colorAttachments[i];
        ca.texture = device_->lookupTexture(info.color_targets[i]);
        ca.loadAction = toMtlLoadAction(info.color_load_op);
        ca.storeAction = MTLStoreActionStore;
        if (info.color_load_op == RhiLoadOp::CLEAR) {
          ca.clearColor = MTLClearColorMake(
              info.clear_color[0], info.clear_color[1],
              info.clear_color[2], info.clear_color[3]);
        }
      }
    }

    void configureDepthAttachment(MTLRenderPassDescriptor* rpd,
                                  const RhiRenderPassBeginInfo& info) {
      if (info.depth_target == RHI_TEXTURE_INVALID) { return; }
      rpd.depthAttachment.texture = device_->lookupTexture(info.depth_target);
      rpd.depthAttachment.loadAction = toMtlLoadAction(info.depth_load_op);
      rpd.depthAttachment.storeAction = MTLStoreActionStore;
      rpd.depthAttachment.clearDepth = info.clear_depth;
    }

    void applyBoundPipeline() {
      if (render_enc_ == nil || bound_.render_pso == nil) { return; }
      [render_enc_ setRenderPipelineState:bound_.render_pso];
      if (bound_.depth_stencil != nil) {
        [render_enc_ setDepthStencilState:bound_.depth_stencil];
      }
      [render_enc_
          setTriangleFillMode:toMtlFillMode(bound_.raster.wireframe
                                                 ? MtlTriangleFill::WIREFRAME
                                                 : MtlTriangleFill::FILLED)];
      [render_enc_ setCullMode:toMtlCullMode(bound_.raster.cull_back
                                                 ? MtlBackFaceCull::CULL_BACK
                                                 : MtlBackFaceCull::NONE)];
      [render_enc_
          setFrontFacingWinding:toMtlWinding(
              bound_.raster.front_ccw ? MtlFrontFaceWinding::COUNTER_CLOCKWISE
                                      : MtlFrontFaceWinding::CLOCKWISE)];
    }

    /// Metal command buffer for this recording session.
    id<MTLCommandBuffer> cmd_buffer_ = nil;
    /// Owning device used for resource lookups.
    MetalRealDevice* device_ = nullptr;
    /// Active render command encoder (nil when outside a render pass).
    id<MTLRenderCommandEncoder> render_enc_ = nil;
    /// Currently bound pipeline state.
    PipelineEntry bound_{};
    /// Currently bound index buffer.
    id<MTLBuffer> idx_buf_ = nil;
    /// Byte offset into the bound index buffer.
    uint64_t idx_off_ = 0;
    /// Metal index type for the bound index buffer.
    MTLIndexType idx_type_ = MTLIndexTypeUInt16;
  };

}  // namespace

// =========================================================================
// MetalRealDevice — Factory
// =========================================================================

std::optional<std::unique_ptr<RhiDevice>>
MetalRealDevice::tryCreate(const MetalRhiConfig& config) {
  @autoreleasepool {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (device == nil) { return std::nullopt; }
    auto result = std::unique_ptr<MetalRealDevice>(new MetalRealDevice(config));
    if (!result->initGpuResources(device)) { return std::nullopt; }
    return std::unique_ptr<RhiDevice>(std::move(result));
  }
}

bool MetalRealDevice::initGpuResources(id<MTLDevice> device) {
  device_ = device;
  queue_ = [device newCommandQueue];
  unified_memory_ = [device hasUnifiedMemory];
  auto* window = static_cast<SDL_Window*>(config_.native_window);
  metal_view_ = SDL_Metal_CreateView(window);
  if (metal_view_ == nullptr) { return false; }
  configureLayer(device);
  backbuffer_handle_ = next_handle_++;
  frame_sem_ = dispatch_semaphore_create(config_.max_frames_in_flight);
  populateCapabilities();
  return true;
}

// =========================================================================
// MetalRealDevice — Lifecycle
// =========================================================================

MetalRealDevice::MetalRealDevice(const MetalRhiConfig& config)
  : config_(config) {}

MetalRealDevice::~MetalRealDevice() {
  @autoreleasepool {
    if (metal_view_ != nullptr) {
      SDL_Metal_DestroyView(metal_view_);
    }
  }
}

void MetalRealDevice::populateCapabilities() {
  caps_.backend = RhiBackend::METAL;
  caps_.compute_supported = true;
  caps_.ray_tracing_supported = [device_ supportsRaytracing];
  caps_.max_buffer_size = [device_ maxBufferLength];
  caps_.max_texture_dimension_2d = MAX_TEXTURE_DIM_2D;
  caps_.max_bound_descriptor_sets = METAL_MAX_DESCRIPTOR_SETS;
  caps_.device_name = [[device_ name] UTF8String];
  caps_.api_version = "Metal 3";
}

void MetalRealDevice::configureLayer(id<MTLDevice> device) {
  // __bridge is Objective-C ARC bridge syntax, not a C-style cast.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
  auto* layer = (__bridge CAMetalLayer*)SDL_Metal_GetLayer(metal_view_);
#pragma clang diagnostic pop
  layer.device = device;
  layer.pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
  layer.drawableSize = CGSizeMake(config_.backbuffer_width,
                                  config_.backbuffer_height);
  layer.displaySyncEnabled = config_.vsync;
  layer_ = layer;
}

// =========================================================================
// MetalRealDevice — Buffer Lifecycle
// =========================================================================

RhiBufferHandle MetalRealDevice::createBuffer(const RhiBufferDesc& desc) {
  @autoreleasepool {
    auto options = bufferStorageMode(desc.host_visible, unified_memory_);
    id<MTLBuffer> buf = [device_ newBufferWithLength:desc.size options:options];
    if (buf == nil) { return RHI_BUFFER_INVALID; }

    if (desc.debug_name != nullptr) {
      buf.label = [NSString stringWithUTF8String:desc.debug_name];
    }

    auto h = next_handle_++;
    buffers_.insert(h, buf);
    buffer_host_visible_.insert(h, desc.host_visible);
    return h;
  }
}

void MetalRealDevice::destroyBuffer(RhiBufferHandle h) {
  buffers_.erase(h);
  buffer_host_visible_.erase(h);
}

void* MetalRealDevice::mapBuffer(RhiBufferHandle h) {
  id<MTLBuffer> buf = buffers_.lookup(h);
  return buf != nil ? [buf contents] : nullptr;
}

void MetalRealDevice::unmapBuffer(RhiBufferHandle h) {
  @autoreleasepool {
    if (!unified_memory_) {
      id<MTLBuffer> buf = buffers_.lookup(h);
      if (buf != nil) {
        [buf didModifyRange:NSMakeRange(0, [buf length])];
      }
    }
  }
}

// =========================================================================
// MetalRealDevice — Texture Lifecycle
// =========================================================================

MTLTextureDescriptor*
MetalRealDevice::buildTextureDescriptor(const RhiTextureDesc& desc) {
  auto* td = [[MTLTextureDescriptor alloc] init];
  td.textureType = MTLTextureType2D;
  td.pixelFormat = toMtlPixelFormat(desc.format);
  td.width = desc.width;
  td.height = desc.height;
  td.depth = desc.depth;
  td.mipmapLevelCount = desc.mip_levels;
  td.arrayLength = desc.array_layers;
  td.usage = toMtlTextureUsage(desc.usage);
  td.storageMode = MTLStorageModePrivate;
  return td;
}

RhiTextureHandle MetalRealDevice::createTexture(const RhiTextureDesc& desc) {
  @autoreleasepool {
    MTLTextureDescriptor* td = buildTextureDescriptor(desc);
    // replaceRegion:withBytes: requires Shared or Managed storage; Private is
    // GPU-only and triggers undefined behaviour / crashes when CPU-uploading.
    if (desc.initial_pixels != nullptr) {
      td.storageMode = MTLStorageModeShared;
    }
    id<MTLTexture> tex = [device_ newTextureWithDescriptor:td];
    if (tex == nil) { return RHI_TEXTURE_INVALID; }
    if (desc.debug_name != nullptr) {
      tex.label = [NSString stringWithUTF8String:desc.debug_name];
    }
    auto h = next_handle_++;
    textures_.insert(h, tex);
    if (desc.initial_pixels != nullptr) { uploadInitialPixels(tex, desc); }
    return h;
  }
}

void MetalRealDevice::destroyTexture(RhiTextureHandle h) { textures_.erase(h); }

bool MetalRealDevice::updateTexture2D(RhiTextureHandle h,
                                        const RhiTextureUpdate2D& u) {
  @autoreleasepool {
    id<MTLTexture> tex = textures_.lookup(h);
    if (tex == nil || u.pixels == nullptr || u.width == 0 || u.height == 0) {
      return false;
    }
    if (u.format == RhiFormat::UNDEFINED) {
      return false;
    }
    if ([tex storageMode] == MTLStorageModePrivate) {
      return false;
    }
    const auto tw = static_cast<uint32_t>([tex width]);
    const auto th = static_cast<uint32_t>([tex height]);
    if (u.offset_x + u.width > tw || u.offset_y + u.height > th) {
      return false;
    }
    const MTLPixelFormat expected_pf = toMtlPixelFormat(u.format);
    if (expected_pf == MTLPixelFormatInvalid ||
        [tex pixelFormat] != expected_pf) {
      return false;
    }
    const uint32_t bpp = bytesPerTexel(u.format);
    if (bpp == 0) {
      return false;
    }
    uint32_t row_b = u.bytes_per_row;
    if (row_b == 0) {
      row_b = u.width * bpp;
    }
    if (row_b < u.width * bpp || row_b % bpp != 0) {
      return false;
    }
    MTLRegion region =
        MTLRegionMake2D(u.offset_x, u.offset_y, u.width, u.height);
    [tex replaceRegion:region mipmapLevel:0 withBytes:u.pixels
           bytesPerRow:row_b];
    return true;
  }
}

// =========================================================================
// MetalRealDevice — Shader Lifecycle
// =========================================================================

ShaderEntry MetalRealDevice::compileShader(const RhiShaderDesc& desc) {
  auto data = dispatch_data_create(
      desc.bytecode, desc.bytecode_size, nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
  NSError* err = nil;
  id<MTLLibrary> lib = [device_ newLibraryWithData:data error:&err];
  if (lib == nil) { return {}; }
  auto* name = [NSString stringWithUTF8String:desc.entry_point];
  id<MTLFunction> func = [lib newFunctionWithName:name];
  if (func == nil) { return {}; }
  return {lib, func};
}

RhiShaderHandle MetalRealDevice::createShader(const RhiShaderDesc& desc) {
  @autoreleasepool {
    auto entry = compileShader(desc);
    if (entry.function == nil) { return RHI_SHADER_INVALID; }
    auto h = next_handle_++;
    shaders_.insert(h, std::move(entry));
    return h;
  }
}

void MetalRealDevice::destroyShader(RhiShaderHandle h) { shaders_.erase(h); }

// =========================================================================
// MetalRealDevice — Pipeline Lifecycle
// =========================================================================

RhiPipelineHandle MetalRealDevice::createGraphicsPipeline(
    const RhiGraphicsPipelineDesc& desc) {
  @autoreleasepool {
    auto vs = shaders_.lookup(desc.vertex_shader);
    auto fs = shaders_.lookup(desc.fragment_shader);
    if (vs.function == nil) { return RHI_PIPELINE_INVALID; }

    auto* pd = buildPipelineDescriptor(vs, fs, desc);
    NSError* error = nil;
    auto pso = [device_ newRenderPipelineStateWithDescriptor:pd error:&error];
    if (pso == nil) { return RHI_PIPELINE_INVALID; }

    auto dss = makeMtlDepthStencilState(device_, desc.depth_stencil);
    auto h = next_handle_++;
    pipelines_.insert(h, PipelineEntry{pso, nil, dss, desc.topology, desc.raster});
    return h;
  }
}

RhiPipelineHandle MetalRealDevice::createComputePipeline(
    const RhiComputePipelineDesc& desc) {
  @autoreleasepool {
    auto cs = shaders_.lookup(desc.compute_shader);
    if (cs.function == nil) { return RHI_PIPELINE_INVALID; }

    NSError* error = nil;
    auto pso = [device_ newComputePipelineStateWithFunction:cs.function
                                                     error:&error];
    if (pso == nil) { return RHI_PIPELINE_INVALID; }

    auto h = next_handle_++;
    pipelines_.insert(h, PipelineEntry{nil, pso, nil});
    return h;
  }
}

void MetalRealDevice::destroyPipeline(RhiPipelineHandle h) {
  pipelines_.erase(h);
}

bool MetalRealDevice::insertGuiPipelineFromPso(id<MTLRenderPipelineState> pso,
                                               RhiPipelineHandle& out) {
  RhiDepthStencilState ds{};
  ds.depth_test = false;
  ds.depth_write = false;
  id<MTLDepthStencilState> dss = makeMtlDepthStencilState(device_, ds);
  RhiRasterState raster{};
  raster.cull_back = false;
  const auto h = next_handle_++;
  pipelines_.insert(
      h, PipelineEntry{pso, nil, dss, RhiPrimitiveTopology::TRIANGLE_LIST, raster});
  out = h;
  return true;
}

bool MetalRealDevice::tryCreateGuiPipeline(RhiPipelineHandle& out) {
  @autoreleasepool {
    id<MTLRenderPipelineState> pso = nil;
    if (!buildGuiPipelinePso(device_, &pso)) {
      return false;
    }
    return insertGuiPipelineFromPso(pso, out);
  }
}

bool MetalRealDevice::insertMeshPipelineFromPso(id<MTLRenderPipelineState> pso,
                                                RhiPipelineHandle& out) {
  // Unlike the GUI pipeline this one tests and writes depth, which is what
  // resolves a mesh against another mesh with no CPU-side sorting.
  RhiDepthStencilState ds{};
  ds.depth_test = true;
  ds.depth_write = true;
  RhiRasterState raster{};
  // OBJ files in the wild disagree about winding; culling would drop half of
  // some models entirely. Showing the geometry beats saving the fragments.
  raster.cull_back = false;
  const auto h = next_handle_++;
  pipelines_.insert(h, PipelineEntry{pso, nil,
                                     makeMtlDepthStencilState(device_, ds),
                                     RhiPrimitiveTopology::TRIANGLE_LIST,
                                     raster});
  out = h;
  return true;
}

bool MetalRealDevice::tryCreateMeshPipeline(RhiPipelineHandle& out) {
  @autoreleasepool {
    id<MTLRenderPipelineState> pso = nil;
    if (!buildMeshPipelinePso(device_, &pso)) {
      return false;
    }
    return insertMeshPipelineFromPso(pso, out);
  }
}

// =========================================================================
// MetalRealDevice — Swap Chain
// =========================================================================

RhiTextureHandle MetalRealDevice::backbufferTexture() const {
  return backbuffer_handle_;
}

uint32_t MetalRealDevice::backbufferWidth() const {
  return config_.backbuffer_width;
}

uint32_t MetalRealDevice::backbufferHeight() const {
  return config_.backbuffer_height;
}

void MetalRealDevice::resizeSwapchain(uint32_t width, uint32_t height) {
  if (width == 0U || height == 0U) {
    return;
  }
  config_.backbuffer_width = width;
  config_.backbuffer_height = height;
  if (layer_ != nil) {
    layer_.drawableSize = CGSizeMake(width, height);
  }
}

// =========================================================================
// MetalRealDevice — Frame Management
// =========================================================================

bool MetalRealDevice::beginFrame() {
  @autoreleasepool {
    dispatch_semaphore_wait(frame_sem_, DISPATCH_TIME_FOREVER);
    drawable_ = [layer_ nextDrawable];
    if (drawable_ == nil) {
      dispatch_semaphore_signal(frame_sem_);
      return false;
    }
    backbuffer_texture_ = drawable_.texture;
    textures_.insert(backbuffer_handle_, backbuffer_texture_);
    return true;
  }
}

void MetalRealDevice::endFrame() {
  if (last_cmd_buffer_ == nil) { return; }
  __block dispatch_semaphore_t sem = frame_sem_;
  [last_cmd_buffer_ addCompletedHandler:^(id<MTLCommandBuffer>) {
    dispatch_semaphore_signal(sem);
  }];
}

void MetalRealDevice::submit(RhiCommandList&) {}

bool MetalRealDevice::present() {
  @autoreleasepool {
    if (drawable_ == nil || last_cmd_buffer_ == nil) { return false; }
    [last_cmd_buffer_ presentDrawable:drawable_];
    [last_cmd_buffer_ commit];
    drawable_ = nil;
    last_cmd_buffer_ = nil;
    return true;
  }
}

std::unique_ptr<RhiCommandList> MetalRealDevice::createCommandList() {
  @autoreleasepool {
    id<MTLCommandBuffer> cb = [queue_ commandBuffer];
    last_cmd_buffer_ = cb;
    return std::make_unique<MetalRealCommandList>(cb, this);
  }
}

// =========================================================================
// MetalRealDevice — Capture API
// =========================================================================

id<MTLTexture>
MetalRealDevice::resolveCaptureTex(const RhiCaptureRequest& request) {
  if (request.target != RHI_TEXTURE_INVALID) {
    return textures_.lookup(request.target);
  }
  return backbuffer_texture_;
}

void MetalRealDevice::blitTextureToBuffer(const RhiCaptureBlitParams& params) {
  id<MTLCommandBuffer> cb = [queue_ commandBuffer];
  id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
  encodeTextureToBufBlit(blit, params);
  [blit endEncoding];
  [cb commit];
  [cb waitUntilCompleted];
}

std::optional<RhiCaptureResult>
MetalRealDevice::captureFramebuffer(const RhiCaptureRequest& request) {
  @autoreleasepool {
    id<MTLTexture> tex = resolveCaptureTex(request);
    if (tex == nil) { return std::nullopt; }
    const uint32_t w = static_cast<uint32_t>([tex width]);
    const uint32_t h = static_cast<uint32_t>([tex height]);
    const auto sz = static_cast<size_t>(w) * h * BACKBUFFER_BYTES_PER_PIXEL;
    auto buf = [device_ newBufferWithLength:sz
                                    options:MTLResourceStorageModeShared];
    if (buf == nil) { return std::nullopt; }
    blitTextureToBuffer({tex, buf, w, h});
    return buildCaptureResult(buf, {tex, buf, w, h}, request);
  }
}

bool MetalRealDevice::captureToFile(const RhiCaptureRequest& request,
                                    std::string_view path) {
  auto result = captureFramebuffer(request);
  if (!result.has_value()) { return false; }
  std::ofstream file(std::string(path), std::ios::binary);
  if (!file.is_open()) { return false; }
  file.write(reinterpret_cast<const char*>(result->data.data()),
             static_cast<std::streamsize>(result->data.size()));
  return file.good();
}

// =========================================================================
// MetalRealDevice — Synchronization + Lookups
// =========================================================================

void MetalRealDevice::waitIdle() {
  @autoreleasepool {
    id<MTLCommandBuffer> cb = [queue_ commandBuffer];
    [cb commit];
    [cb waitUntilCompleted];
  }
}

id<MTLBuffer> MetalRealDevice::lookupBuffer(RhiBufferHandle h) const {
  return buffers_.lookup(h);
}

id<MTLTexture> MetalRealDevice::lookupTexture(RhiTextureHandle h) const {
  return textures_.lookup(h);
}

PipelineEntry MetalRealDevice::lookupPipeline(RhiPipelineHandle h) const {
  return pipelines_.lookup(h);
}

// =========================================================================
// MetalRhiDevice wrapper + factory
// =========================================================================

struct MetalRhiDevice::Impl {
  /// Whether the device reports unified CPU/GPU memory.
  bool unified_memory = false;
};

MetalRhiDevice::MetalRhiDevice() = default;
MetalRhiDevice::~MetalRhiDevice() = default;

MetalRhiDevice::MetalRhiDevice(MetalRhiDevice&& other) noexcept
  : impl_(std::move(other.impl_)) {}

MetalRhiDevice& MetalRhiDevice::operator=(MetalRhiDevice&& other) noexcept {
  impl_ = std::move(other.impl_);
  return *this;
}

std::optional<std::unique_ptr<RhiDevice>>
MetalRhiDevice::create(const MetalRhiConfig& config) {
  if (config.native_window == nullptr) {
    return std::make_unique<MetalStubDevice>(config);
  }
  auto result = MetalRealDevice::tryCreate(config);
  if (result.has_value()) { return result; }
  return std::make_unique<MetalStubDevice>(config);
}

bool MetalRhiDevice::hasUnifiedMemory() const {
  if (!impl_) { return false; }
  return impl_->unified_memory;
}

// =========================================================================
// MetalRhiCommandList wrapper
// =========================================================================

struct MetalRhiCommandList::Impl {};

MetalRhiCommandList::MetalRhiCommandList() = default;
MetalRhiCommandList::~MetalRhiCommandList() = default;

MetalRhiCommandList::MetalRhiCommandList(MetalRhiCommandList&& other) noexcept
  : impl_(std::move(other.impl_)) {}

MetalRhiCommandList&
MetalRhiCommandList::operator=(MetalRhiCommandList&& other) noexcept {
  impl_ = std::move(other.impl_);
  return *this;
}

}  // namespace eng
