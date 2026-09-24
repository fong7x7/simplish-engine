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
#include <engine/render/rhi-shader-desc.h>
#include <engine/render/rhi-texture-update-2d.h>
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
  std::vector<uint8_t> encodePixels(const uint8_t* data, uint32_t w, uint32_t h,
                                    RhiCaptureFormat fmt,
                                    uint8_t jpeg_quality) {
    auto stride = static_cast<int>(w) * CAPTURE_CHANNELS;
    std::vector<uint8_t> encoded;
    if (fmt == RhiCaptureFormat::JPEG) {
      stbi_write_jpg_to_func(stbWriteCallback, &encoded, static_cast<int>(w),
                             static_cast<int>(h), CAPTURE_CHANNELS, data,
                             static_cast<int>(jpeg_quality));
    } else {
      stbi_write_png_to_func(stbWriteCallback, &encoded, static_cast<int>(w),
                             static_cast<int>(h), CAPTURE_CHANNELS, data,
                             stride);
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
  MTLResourceOptions bufferStorageMode(bool host_visible, bool unified_memory) {
    if (host_visible && unified_memory) {
      return MTLResourceStorageModeShared;
    }
    if (host_visible) {
      return MTLResourceStorageModeManaged;
    }
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
    const size_t count =
        static_cast<size_t>(params.width) * params.height * CAPTURE_CHANNELS;
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
  MTLVertexDescriptor* buildVertexDescriptor(const RhiVertexLayout& layout) {
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
    [blit copyFromTexture:params.texture
                     sourceSlice:0
                     sourceLevel:0
                    sourceOrigin:MTLOriginMake(0, 0, 0)
                      sourceSize:MTLSizeMake(params.width, params.height, 1)
                        toBuffer:params.buffer
               destinationOffset:0
          destinationBytesPerRow:row
        destinationBytesPerImage:img];
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
    if (desc.blend.enabled) {
      applyAlphaBlend(pd.colorAttachments[0]);
    }
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
  constexpr NSUInteger MESH_VERTEX_STRIDE = 32;
  /// Byte offset of each mesh vertex attribute, matching `mesh-vertex.h`.
  constexpr NSUInteger MESH_NORMAL_OFFSET = 12;
  constexpr NSUInteger MESH_UV_OFFSET = 24;

  /// MSL source for static meshes (matches `eng::MeshVertex`).
  ///
  /// The vertex stage takes the world-to-clip and object-to-world matrices
  /// as vertex stage bytes at slot 1, and the fragment stage takes the
  /// level's lights at slot 0, so a draw needs no descriptor set. The
  /// structures here mirror `eng::MeshLight` and the block
  /// `mesh-renderer.cpp` builds around it, register for register — see
  /// `mesh-light.h` for why they are laid out as `float4`s.
  ///
  /// Shading is ambient plus a lambert term per light, which is what
  /// `mesh-rasterizer.cpp` computes on the CPU for thumbnails and tests. The
  /// two are meant to agree; neither is the real lighting model that
  /// [Engine REQUIREMENTS §5.4] describes.
  constexpr const char MESH_MSL_SOURCE[] = R"msl(
#include <metal_stdlib>
using namespace metal;

// These four are `mesh-light.h`'s constants, restated because MSL cannot
// include a C++ header. MESH_MAX_LIGHTS sizes the array below, so it and the
// C++ one have to move together; the fifth is `mesh-alpha-cutoff.h`'s.
constant uint MESH_MAX_LIGHTS = 8;
constant float MESH_LIGHT_AMBIENT = 0.38f;
constant float MESH_LIGHT_DIFFUSE = 0.62f;
constant float MESH_LIGHT_POINT = 1.0f;
// And this one is `mesh-alpha-cutoff.h`'s.
constant float MESH_ALPHA_CUTOFF = 0.5f;

struct MeshUniforms {
  float4x4 view_projection;
  float4x4 model;
};

struct MeshLight {
  float4 position_range;
  float4 direction_intensity;
  float4 color_kind;
};

struct MeshLights {
  uint count;
  uint shade_bands;
  uint pad1;
  uint pad2;
  MeshLight lights[MESH_MAX_LIGHTS];
};

struct MeshVertexIn {
  float3 position [[attribute(0)]];
  float3 normal [[attribute(1)]];
  float2 uv [[attribute(2)]];
};

struct MeshVsOut {
  float4 position [[position]];
  float3 world_position;
  float3 normal;
  float2 uv;
};

/// Linear value for an sRGB colour component, for output to an sRGB target.
float mesh_srgb_to_linear(float srgb) {
  if (srgb <= 0.04045f) {
    return srgb / 12.92f;
  }
  return pow((srgb + 0.055f) / 1.055f, 2.4f);
}

/// How much of a point light reaches a surface this far from it: full at the
/// light, nothing at its range, and squared in between so the falloff reads
/// as light rather than as a gradient.
float mesh_falloff(float distance, float range) {
  if (range <= 0.0f) {
    return 0.0f;
  }
  float reach = saturate(1.0f - distance / range);
  return reach * reach;
}

/// One light's strength flattened into `bands` tones, or left alone for
/// fewer than two. `meshShadeBand` in `mesh-style.h`, restated.
float mesh_band(float light, uint bands) {
  if (bands < 2u) {
    return light;
  }
  float top = float(bands - 1u);
  return min(floor(light * float(bands)), top) / top;
}

/// What one light adds to a surface.
float3 mesh_light_contribution(MeshLight light, float3 world_position,
                               float3 normal, uint bands) {
  float3 to_light = light.direction_intensity.xyz;
  float attenuation = 1.0f;
  if (light.color_kind.w == MESH_LIGHT_POINT) {
    float3 offset = light.position_range.xyz - world_position;
    attenuation = mesh_falloff(length(offset), light.position_range.w);
    to_light = offset;
  }
  // A light aimed nowhere lights nothing, rather than dividing by zero.
  float aim = length(to_light);
  if (aim < 1e-4f || attenuation <= 0.0f) {
    return float3(0.0f);
  }
  float lambert = saturate(dot(normal, to_light / aim));
  return light.color_kind.xyz * light.direction_intensity.w *
         mesh_band(lambert * attenuation, bands) * MESH_LIGHT_DIFFUSE;
}

vertex MeshVsOut mesh_vs_main(MeshVertexIn in [[stage_in]],
                              constant MeshUniforms& u [[buffer(1)]]) {
  MeshVsOut out;
  float4 world = u.model * float4(in.position, 1.0f);
  out.position = u.view_projection * world;
  out.world_position = world.xyz;
  // The placement transform is a rotation and a uniform scale, so the same
  // matrix carries the normal; the length the scale adds comes back out in
  // the normalize below.
  out.normal = (u.model * float4(in.normal, 0.0f)).xyz;
  out.uv = in.uv;
  return out;
}

// ---------------------------------------------------------------------------
// Skinned meshes: a different vertex stage in front of the same fragment one.
// SKIN_MAX_JOINTS is `MESH_MAX_SKIN_JOINTS` in `skin-palette.h`, restated;
// the palette is `SkinPalette`, three rows of each joint's matrix.
// ---------------------------------------------------------------------------
constant uint SKIN_MAX_JOINTS = 80;

struct SkinnedVertexIn {
  float3 position [[attribute(0)]];
  float3 normal [[attribute(1)]];
  float2 uv [[attribute(2)]];
  uchar4 joints [[attribute(3)]];
  float4 weights [[attribute(4)]];
};

struct SkinPalette {
  float4 rows[SKIN_MAX_JOINTS * 3];
};

vertex MeshVsOut skinned_vs_main(SkinnedVertexIn in [[stage_in]],
                                 constant MeshUniforms& u [[buffer(1)]],
                                 constant SkinPalette& palette [[buffer(2)]]) {
  // Linear blend skinning: the weighted sum of the joints' matrices, built a
  // row at a time, which is what `poseSkinnedMesh` does on the CPU.
  float4 r0 = float4(0.0f);
  float4 r1 = float4(0.0f);
  float4 r2 = float4(0.0f);
  for (uint i = 0; i < 4; ++i) {
    uint j = min(uint(in.joints[i]), SKIN_MAX_JOINTS - 1u) * 3u;
    r0 += palette.rows[j] * in.weights[i];
    r1 += palette.rows[j + 1u] * in.weights[i];
    r2 += palette.rows[j + 2u] * in.weights[i];
  }
  float4 p = float4(in.position, 1.0f);
  float4 n = float4(in.normal, 0.0f);
  float3 skinned_position = float3(dot(r0, p), dot(r1, p), dot(r2, p));
  float3 skinned_normal = float3(dot(r0, n), dot(r1, n), dot(r2, n));
  MeshVsOut out;
  float4 world = u.model * float4(skinned_position, 1.0f);
  out.position = u.view_projection * world;
  out.world_position = world.xyz;
  out.normal = (u.model * float4(skinned_normal, 0.0f)).xyz;
  out.uv = in.uv;
  return out;
}

fragment float4 mesh_fs_main(MeshVsOut in [[stage_in]],
                             constant MeshLights& lights [[buffer(0)]],
                             texture2d<float> diffuse [[texture(0)]]) {
  constexpr sampler smp(filter::linear, address::repeat);
  float3 n = normalize(in.normal);
  float3 lit = float3(MESH_LIGHT_AMBIENT);
  for (uint i = 0; i < lights.count && i < MESH_MAX_LIGHTS; ++i) {
    lit += mesh_light_contribution(lights.lights[i], in.world_position, n,
                                   lights.shade_bands);
  }
  // The map is unorm, so this is the sRGB value the artist authored, shaded
  // and then converted on the way out — which is what the flat colour this
  // replaced did. An instance with no map samples one texel of that same
  // flat colour, so there is no untextured branch here.
  float4 map = diffuse.sample(smp, in.uv);
  // Alpha-test cutout (ADR-003), which is what lets a sprite billboard go
  // through this pass: its empty corners have to not draw, and the pass is
  // opaque and depth-writing, so the only way for them not to is for their
  // fragments not to exist. Opaque geometry never reaches the branch.
  if (map.a < MESH_ALPHA_CUTOFF) {
    discard_fragment();
  }
  float3 base = saturate(map.rgb * lit);
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
    vd.attributes[1].offset = MESH_NORMAL_OFFSET;
    vd.attributes[1].bufferIndex = 0;
    vd.attributes[2].format = MTLVertexFormatFloat2;
    vd.attributes[2].offset = MESH_UV_OFFSET;
    vd.attributes[2].bufferIndex = 0;
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
                                       id<MTLFunction> vs, id<MTLFunction> fs) {
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

  /// Byte stride for `SkinnedMeshVertex`: a `MeshVertex`, four joint bytes,
  /// and four float weights. `skinned-mesh-vertex.h` asserts the same three
  /// numbers.
  constexpr NSUInteger SKINNED_VERTEX_STRIDE = 52;
  constexpr NSUInteger SKINNED_JOINTS_OFFSET = 32;
  constexpr NSUInteger SKINNED_WEIGHTS_OFFSET = 36;

  /// The static mesh's three attributes, then the joints and weights.
  MTLVertexDescriptor* makeSkinnedVertexDescriptor() {
    MTLVertexDescriptor* vd = makeMeshVertexDescriptor();
    vd.layouts[0].stride = SKINNED_VERTEX_STRIDE;
    vd.attributes[3].format = MTLVertexFormatUChar4;
    vd.attributes[3].offset = SKINNED_JOINTS_OFFSET;
    vd.attributes[3].bufferIndex = 0;
    vd.attributes[4].format = MTLVertexFormatFloat4;
    vd.attributes[4].offset = SKINNED_WEIGHTS_OFFSET;
    vd.attributes[4].bufferIndex = 0;
    return vd;
  }

  /// The skinned pipeline: `skinned_vs_main` in front of the static mesh's
  /// own fragment function, from the same library, so the two shade alike
  /// by construction rather than by keeping two copies in step.
  bool buildSkinnedMeshPipelinePso(id<MTLDevice> mtl_device,
                                   id<MTLRenderPipelineState>* out_pso) {
    id<MTLLibrary> lib = nil;
    if (!compileMeshShaderLibrary(mtl_device, &lib)) {
      return false;
    }
    id<MTLFunction> vs = [lib newFunctionWithName:@"skinned_vs_main"];
    id<MTLFunction> fs = [lib newFunctionWithName:@"mesh_fs_main"];
    if (vs == nil || fs == nil) {
      return false;
    }
    auto* pd = [[MTLRenderPipelineDescriptor alloc] init];
    configureMeshRenderPipelineDesc(pd, vs, fs);
    pd.vertexDescriptor = makeSkinnedVertexDescriptor();
    NSError* err = nil;
    *out_pso = [mtl_device newRenderPipelineStateWithDescriptor:pd error:&err];
    (void)err;
    return *out_pso != nil;
  }

  /// MSL for the mesh outline: one full-screen triangle, and a fragment
  /// stage that reads the scene's depth and draws a line wherever it bends.
  /// `mesh-outline-renderer.h` explains the method. `OutlineUniforms` below
  /// is the C++ struct of that name in `mesh-outline-renderer.cpp`, which
  /// this cannot include; OUTLINE_HLSL_SOURCE and the GLSL mirror it.
  constexpr const char OUTLINE_MSL_SOURCE[] = R"msl(
#include <metal_stdlib>
using namespace metal;

struct OutlineUniforms {
  float4 color;
  float4 bounds;
  float width;
  float threshold;
  float pad0;
  float pad1;
};

struct OutlineVsOut {
  float4 position [[position]];
};

vertex OutlineVsOut outline_vs_main(uint id [[vertex_id]]) {
  // (-1,-1), (3,-1) and (-1,3): one triangle covering all of clip space,
  // so there is no seam down a diagonal and no vertex buffer to bind.
  float2 corner = float2(float((id << 1u) & 2u), float(id & 2u));
  OutlineVsOut out;
  out.position = float4(corner * 2.0f - 1.0f, 0.0f, 1.0f);
  return out;
}

/// Depth at a pixel, held inside the scissor so that nothing outside what
/// the scene drew into is read, and a mesh cut off by it is not lined.
float outline_depth_at(depth2d<float> depth, constant OutlineUniforms& u,
                       int2 p) {
  int2 lo = int2(u.bounds.xy);
  int2 hi = int2(u.bounds.zw) - 1;
  return depth.read(uint2(clamp(p, lo, hi)));
}

fragment float4 outline_fs_main(OutlineVsOut in [[stage_in]],
                                constant OutlineUniforms& u [[buffer(0)]],
                                depth2d<float> depth [[texture(0)]]) {
  int2 p = int2(in.position.xy);
  float centre = outline_depth_at(depth, u, p);
  // Nothing was drawn here, so there is nothing to outline.
  if (centre >= 1.0f) {
    discard_fragment();
  }
  int w = int(u.width);
  float across = outline_depth_at(depth, u, p + int2(w, 0)) +
                 outline_depth_at(depth, u, p - int2(w, 0)) - 2.0f * centre;
  float down = outline_depth_at(depth, u, p + int2(0, w)) +
               outline_depth_at(depth, u, p - int2(0, w)) - 2.0f * centre;
  // Positive where this pixel is nearer than its neighbours on average,
  // which is the near side of an edge: the rim of the thing in front.
  float bend = max(across, down);
  if (bend <= u.threshold) {
    discard_fragment();
  }
  float cover = saturate((bend - u.threshold) / max(u.threshold, 1e-9f));
  return float4(u.color.rgb, u.color.a * cover);
}
)msl";

  id<MTLLibrary> compileOutlineShaderLibrary(id<MTLDevice> mtl_device) {
    NSError* err = nil;
    NSString* src = [NSString stringWithUTF8String:OUTLINE_MSL_SOURCE];
    id<MTLLibrary> lib = [mtl_device newLibraryWithSource:src
                                                  options:nil
                                                    error:&err];
    (void)err;
    return lib;
  }

  /// The outline pipeline: no vertex input, no depth attachment, and the
  /// GUI's "over" blend so a softened crease pixel mixes with the scene.
  bool buildOutlinePipelinePso(id<MTLDevice> mtl_device,
                               id<MTLRenderPipelineState>* out_pso) {
    id<MTLLibrary> lib = compileOutlineShaderLibrary(mtl_device);
    id<MTLFunction> vs = [lib newFunctionWithName:@"outline_vs_main"];
    id<MTLFunction> fs = [lib newFunctionWithName:@"outline_fs_main"];
    if (vs == nil || fs == nil) {
      return false;
    }
    auto* pd = [[MTLRenderPipelineDescriptor alloc] init];
    pd.vertexFunction = vs;
    pd.fragmentFunction = fs;
    pd.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
    applyGuiAlphaBlend(pd.colorAttachments[0]);
    NSError* err = nil;
    *out_pso = [mtl_device newRenderPipelineStateWithDescriptor:pd error:&err];
    (void)err;
    return *out_pso != nil;
  }

  /// MSL for effects particles. `fx-renderer.h` explains the method: each
  /// vertex arrives already in clip space, and the fragment stage reads the
  /// scene's depth under it to hide the particle behind geometry and fade
  /// it just in front. `FxVertexIn` is `FxVertex` in `fx-vertex.h` and
  /// `FxUniforms` the struct of that name in `fx-renderer.cpp`, neither of
  /// which this can include; FX_HLSL_SOURCE and the GLSL mirror it.
  constexpr const char FX_MSL_SOURCE[] = R"msl(
#include <metal_stdlib>
using namespace metal;

struct FxVertexIn {
  float4 clip [[attribute(0)]];
  float4 color [[attribute(1)]];
  float2 uv [[attribute(2)]];
  float2 shape [[attribute(3)]];
};

struct FxUniforms {
  float softness;
  float pad0;
  float pad1;
  float pad2;
};

struct FxVsOut {
  float4 position [[position]];
  float4 color;
  float2 uv;
  float2 shape;
};

vertex FxVsOut fx_vs_main(FxVertexIn in [[stage_in]]) {
  FxVsOut out;
  out.position = in.clip;
  out.color = in.color;
  out.uv = in.uv;
  out.shape = in.shape;
  return out;
}

float fx_hash(float2 p) {
  return fract(sin(dot(p, float2(127.1f, 311.7f))) * 43758.5453f);
}

float fx_noise(float2 p) {
  float2 cell = floor(p);
  float2 f = fract(p);
  float2 s = f * f * (3.0f - 2.0f * f);
  float a = fx_hash(cell);
  float b = fx_hash(cell + float2(1.0f, 0.0f));
  float c = fx_hash(cell + float2(0.0f, 1.0f));
  float d = fx_hash(cell + float2(1.0f, 1.0f));
  return mix(mix(a, b, s.x), mix(c, d, s.x), s.y);
}

float fx_fbm(float2 p) {
  return fx_noise(p) * 0.65f + fx_noise(p * 2.7f + 5.2f) * 0.35f;
}

/// A disc broken up by noise: soft at the rim, uneven inside, and unlike
/// the next particle's, because its seed moves the noise field under it.
float fx_puff(float2 uv, float seed) {
  float edge = saturate(1.0f - length(uv));
  float2 at = uv * 2.3f + float2(seed * 0.37f, seed * 0.71f);
  return saturate(edge * edge * (0.35f + 1.15f * fx_fbm(at)));
}

fragment float4 fx_fs_main(FxVsOut in [[stage_in]],
                           constant FxUniforms& u [[buffer(0)]],
                           depth2d<float> depth [[texture(0)]]) {
  // Behind the scene's nearest surface it is hidden; in front, it fades in
  // over the first stretch of depth rather than being cut off by it.
  float scene = depth.read(uint2(in.position.xy));
  float soft = saturate((scene - in.position.z) * u.softness);
  // A soft disc: full at the centre of the quad, nothing at its edge —
  // or, for smoke and dust, a puff of noise the same size.
  float disc = saturate(1.0f - dot(in.uv, in.uv));
  float shape = mix(disc * disc, fx_puff(in.uv, in.shape.y), in.shape.x);
  float cover = shape * soft;
  if (cover <= 0.0f) {
    discard_fragment();
  }
  return in.color * cover;
}
)msl";

  /// Byte stride of `FxVertex`, and where its colour and uv sit —
  /// `fx-vertex.h` asserts the same three numbers.
  constexpr NSUInteger FX_VERTEX_STRIDE = 48;
  constexpr NSUInteger FX_COLOR_OFFSET = 16;
  constexpr NSUInteger FX_UV_OFFSET = 32;
  constexpr NSUInteger FX_SHAPE_OFFSET = 40;

  MTLVertexDescriptor* makeFxVertexDescriptor() {
    auto* vd = [[MTLVertexDescriptor alloc] init];
    vd.layouts[0].stride = FX_VERTEX_STRIDE;
    vd.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    vd.attributes[0].format = MTLVertexFormatFloat4;
    vd.attributes[0].offset = 0;
    vd.attributes[0].bufferIndex = 0;
    vd.attributes[1].format = MTLVertexFormatFloat4;
    vd.attributes[1].offset = FX_COLOR_OFFSET;
    vd.attributes[1].bufferIndex = 0;
    vd.attributes[2].format = MTLVertexFormatFloat2;
    vd.attributes[2].offset = FX_UV_OFFSET;
    vd.attributes[2].bufferIndex = 0;
    vd.attributes[3].format = MTLVertexFormatFloat2;
    vd.attributes[3].offset = FX_SHAPE_OFFSET;
    vd.attributes[3].bufferIndex = 0;
    return vd;
  }

  /// Premultiplied compositing: the colour is added as it is and the target
  /// kept by what the particle does not hide, so a glow of alpha zero is
  /// purely additive and smoke of alpha one covers what is behind it.
  void applyPremultipliedBlend(MTLRenderPipelineColorAttachmentDescriptor* ca) {
    ca.blendingEnabled = YES;
    ca.rgbBlendOperation = MTLBlendOperationAdd;
    ca.alphaBlendOperation = MTLBlendOperationAdd;
    ca.sourceRGBBlendFactor = MTLBlendFactorOne;
    ca.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    ca.sourceAlphaBlendFactor = MTLBlendFactorOne;
    ca.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
  }

  /// The effects pipeline: `FxVertex` input, no depth attachment — the
  /// depth is the texture being read — and premultiplied blending.
  id<MTLLibrary> compileFxShaderLibrary(id<MTLDevice> mtl_device) {
    NSError* err = nil;
    NSString* src = [NSString stringWithUTF8String:FX_MSL_SOURCE];
    id<MTLLibrary> lib = [mtl_device newLibraryWithSource:src
                                                  options:nil
                                                    error:&err];
    (void)err;
    return lib;
  }

  bool buildFxPipelinePso(id<MTLDevice> mtl_device,
                          id<MTLRenderPipelineState>* out_pso) {
    id<MTLLibrary> lib = compileFxShaderLibrary(mtl_device);
    id<MTLFunction> vs = [lib newFunctionWithName:@"fx_vs_main"];
    id<MTLFunction> fs = [lib newFunctionWithName:@"fx_fs_main"];
    if (vs == nil || fs == nil) {
      return false;
    }
    auto* pd = [[MTLRenderPipelineDescriptor alloc] init];
    pd.vertexFunction = vs;
    pd.fragmentFunction = fs;
    pd.vertexDescriptor = makeFxVertexDescriptor();
    pd.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
    applyPremultipliedBlend(pd.colorAttachments[0]);
    NSError* err = nil;
    *out_pso = [mtl_device newRenderPipelineStateWithDescriptor:pd error:&err];
    (void)err;
    return *out_pso != nil;
  }

  /// The volume pipeline's shaders: a ray marched through a box of noise,
  /// stopped by the scene's depth. `fx-volume-vertex.h` explains what the
  /// five attributes carry and why no matrix is needed here.
  constexpr const char FX_VOLUME_MSL_SOURCE[] = R"msl(
#include <metal_stdlib>
using namespace metal;

struct FxVolumeVertexIn {
  float4 clip [[attribute(0)]];
  float4 color [[attribute(1)]];
  float4 origin [[attribute(2)]];
  float4 ray [[attribute(3)]];
  float4 params [[attribute(4)]];
};

struct FxVolumeVsOut {
  float4 position [[position]];
  float4 color;
  float4 origin;
  float4 ray;
  float4 params;
};

vertex FxVolumeVsOut fx_volume_vs_main(FxVolumeVertexIn in [[stage_in]]) {
  FxVolumeVsOut out;
  out.position = in.clip;
  out.color = in.color;
  out.origin = in.origin;
  out.ray = in.ray;
  out.params = in.params;
  return out;
}

constant int FXV_STEPS = 16;

float fxv_hash(float3 p) {
  return fract(sin(dot(p, float3(127.1f, 311.7f, 74.7f))) * 43758.5453f);
}

float fxv_noise(float3 p) {
  float3 cell = floor(p);
  float3 f = fract(p);
  float3 s = f * f * (3.0f - 2.0f * f);
  float x00 = mix(fxv_hash(cell), fxv_hash(cell + float3(1.0f, 0.0f, 0.0f)),
                  s.x);
  float x10 = mix(fxv_hash(cell + float3(0.0f, 1.0f, 0.0f)),
                  fxv_hash(cell + float3(1.0f, 1.0f, 0.0f)), s.x);
  float x01 = mix(fxv_hash(cell + float3(0.0f, 0.0f, 1.0f)),
                  fxv_hash(cell + float3(1.0f, 0.0f, 1.0f)), s.x);
  float x11 = mix(fxv_hash(cell + float3(0.0f, 1.0f, 1.0f)),
                  fxv_hash(cell + float3(1.0f, 1.0f, 1.0f)), s.x);
  return mix(mix(x00, x10, s.y), mix(x01, x11, s.y), s.z);
}

float fxv_fbm(float3 p) {
  return fxv_noise(p) * 0.6f + fxv_noise(p * 2.3f + 11.0f) * 0.4f;
}

// How thick the smoke is at one point of the cloud's own space: an
// ellipsoid gone to nothing at the box's wall, eaten into by noise that
// the cloud's seed moves, so no two clouds are the same shape.
float fxv_density(float3 p, float seed) {
  float edge = saturate(1.0f - dot(p, p));
  float n = fxv_fbm(p * 1.9f + seed);
  return edge * edge * saturate(n * 1.7f - 0.45f);
}

fragment float4 fx_volume_fs_main(FxVolumeVsOut in [[stage_in]],
                                  depth2d<float> depth [[texture(0)]]) {
  float3 o = in.origin.xyz;
  float3 d = in.ray.xyz;
  float3 inv = 1.0f / d;
  float3 near_wall = (float3(-1.0f) - o) * inv;
  float3 far_wall = (float3(1.0f) - o) * inv;
  float3 lo = min(near_wall, far_wall);
  float3 hi = max(near_wall, far_wall);
  float t_in = max(max(lo.x, lo.y), lo.z);
  // The scene stops the march where a surface is, so the smoke wraps what
  // it meets instead of cutting against it.
  float scene = depth.read(uint2(in.position.xy));
  float t_out = min(min(min(hi.x, hi.y), hi.z),
                    (scene - in.params.x) / in.ray.w);
  if (!(t_out > t_in)) {
    discard_fragment();
  }
  float dt = (t_out - t_in) / float(FXV_STEPS);
  float cover = 0.0f;
  float through = 1.0f;
  for (int i = 0; i < FXV_STEPS; ++i) {
    float3 p = o + d * (t_in + (float(i) + 0.5f) * dt);
    float a = 1.0f - exp(-fxv_density(p, in.origin.w) * in.params.y * dt);
    cover += through * a;
    through *= 1.0f - a;
  }
  if (cover <= 0.0f) {
    discard_fragment();
  }
  return in.color * cover;
}
)msl";

  /// Byte stride of `FxVolumeVertex`, and where each of its float4s sits —
  /// `fx-volume-vertex.h` asserts the same numbers.
  constexpr NSUInteger FX_VOLUME_STRIDE = 80;

  MTLVertexDescriptor* makeFxVolumeVertexDescriptor() {
    auto* vd = [[MTLVertexDescriptor alloc] init];
    vd.layouts[0].stride = FX_VOLUME_STRIDE;
    vd.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    for (NSUInteger i = 0; i < 5; ++i) {
      vd.attributes[i].format = MTLVertexFormatFloat4;
      vd.attributes[i].offset = i * 16;
      vd.attributes[i].bufferIndex = 0;
    }
    return vd;
  }

  bool buildFxVolumePipelinePso(id<MTLDevice> mtl_device,
                                id<MTLRenderPipelineState>* out_pso) {
    NSError* err = nil;
    NSString* src = [NSString stringWithUTF8String:FX_VOLUME_MSL_SOURCE];
    id<MTLLibrary> lib = [mtl_device newLibraryWithSource:src
                                                  options:nil
                                                    error:&err];
    id<MTLFunction> vs = [lib newFunctionWithName:@"fx_volume_vs_main"];
    id<MTLFunction> fs = [lib newFunctionWithName:@"fx_volume_fs_main"];
    if (vs == nil || fs == nil) {
      return false;
    }
    auto* pd = [[MTLRenderPipelineDescriptor alloc] init];
    pd.vertexFunction = vs;
    pd.fragmentFunction = fs;
    pd.vertexDescriptor = makeFxVolumeVertexDescriptor();
    pd.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
    applyPremultipliedBlend(pd.colorAttachments[0]);
    *out_pso = [mtl_device newRenderPipelineStateWithDescriptor:pd error:&err];
    (void)err;
    return *out_pso != nil;
  }

  /// MSL for the water surface: `MeshVertex` triangles in world space, each
  /// carrying its depth in `uv.x`, a `WaterVertexUniforms` block at vertex
  /// buffer 1, a `WaterShading` block at fragment buffer 0, the scene's
  /// lights at fragment buffer 1 in `MESH_MSL_SOURCE`'s own layout, and the
  /// ripple field at texture 0. The structures mirror
  /// `water-vertex-uniforms.h`, `water-shading.h` and `mesh-light.h`, which
  /// this cannot include; WATER_HLSL_SOURCE and the GLSL copies mirror it,
  /// line for line, so the four agree.
  constexpr const char WATER_MSL_SOURCE[] = R"msl(
#include <metal_stdlib>
using namespace metal;

// `water-texels.h`'s and `water-field.h`'s ranges, restated.
constant float WATER_SLOPE_RANGE = 1.0f;
constant float WATER_LEVEL_RANGE = 0.1f;
constant float WATER_SHORE_TILES = 2.0f;
// How much steeper the simulated ripples are drawn than they are: a ring a
// few hundredths of a tile high is what a wake is, and it has to read.
constant float WATER_RIPPLE_GAIN = 3.0f;
// How much sky a ripple's slope towards the eye adds.
constant float WATER_RIPPLE_SKY = 1.2f;
// How much the light the waves focus brightens the ground under them.
constant float WATER_CAUSTIC_LIGHT = 0.25f;
// `water-depth.h`'s bank shelf, restated.
constant float WATER_BANK_MIN_TILES = 0.3f;
constant float WATER_BANK_TILES_PER_DEPTH = 0.5f;
constant float WATER_BANK_MAX_TILES = 2.0f;
// `mesh-light.h`'s, restated, as the mesh shader restates them.
constant uint MESH_MAX_LIGHTS = 8;
constant float MESH_LIGHT_AMBIENT = 0.38f;
constant float MESH_LIGHT_DIFFUSE = 0.62f;
constant float MESH_LIGHT_POINT = 1.0f;

// Wind waves finer than the simulation: a direction, a wavelength in
// tiles, and a steepness — the slope at a crest. The first two are broad
// swell LOW and HIGH draw; the last two only HIGH does.
constant float4 WATER_WAVES[4] = {
    float4(0.80f, 0.60f, 1.10f, 0.10f), float4(-0.45f, 0.89f, 0.63f, 0.08f),
    float4(0.97f, -0.24f, 0.39f, 0.06f), float4(0.20f, 0.98f, 0.25f, 0.05f)};

struct WaterUniforms {
  float4x4 view_projection;
  float4 field;
};

struct WaterShading {
  float4 sky;
  float4 foam;
  float4 clarity;
  float4 light;
  float4 view;
  float4 detail;
};

struct MeshLight {
  float4 position_range;
  float4 direction_intensity;
  float4 color_kind;
};

struct MeshLights {
  uint count;
  uint shade_bands;
  uint pad1;
  uint pad2;
  MeshLight lights[MESH_MAX_LIGHTS];
};

// A surface vertex: where it is, the water's colour in the normal's place,
// and its depth and opacity in the texture coordinate's.
struct WaterVertexIn {
  float3 position [[attribute(0)]];
  float3 normal [[attribute(1)]];
  float2 uv [[attribute(2)]];
};

struct WaterVsOut {
  float4 position [[position]];
  float3 world;
  float2 uv;
  float depth;
  float opacity;
  float3 color;
};

vertex WaterVsOut water_vs_main(WaterVertexIn in [[stage_in]],
                                constant WaterUniforms& u [[buffer(1)]]) {
  WaterVsOut out;
  out.position = u.view_projection * float4(in.position, 1.0f);
  out.world = in.position;
  out.uv = (in.position.xy - u.field.xy) * u.field.zw;
  out.depth = in.uv.x;
  out.opacity = in.uv.y;
  out.color = in.normal;
  return out;
}

float water_srgb_to_linear(float srgb) {
  if (srgb <= 0.04045f) {
    return srgb / 12.92f;
  }
  return pow((srgb + 0.055f) / 1.055f, 2.4f);
}

float3 water_linear(float3 c) {
  c = saturate(c);
  return float3(water_srgb_to_linear(c.r), water_srgb_to_linear(c.g),
                water_srgb_to_linear(c.b));
}

// How much of water `depth` tiles deep there is `shore` tiles from a bank.
float water_shelf(float depth, float shore) {
  float run = clamp(WATER_BANK_MIN_TILES + WATER_BANK_TILES_PER_DEPTH * depth,
                    WATER_BANK_MIN_TILES, WATER_BANK_MAX_TILES);
  return smoothstep(0.0f, 1.0f, saturate(shore / run));
}

// The slope the wind waves add at p, t seconds in. Each travels at the
// speed deep water carries its wavelength, which grows as its root.
float2 water_wind_slope(float2 p, float t, float fine) {
  float2 slope = float2(0.0f);
  for (int i = 0; i < 4; ++i) {
    float4 w = WATER_WAVES[i];
    float k = 6.2831853f / w.z;
    float phase = k * (dot(w.xy, p) - 0.55f * sqrt(w.z) * t);
    float weight = i < 2 ? 1.0f : fine;
    slope += w.xy * (w.w * weight * cos(phase));
  }
  return slope;
}

// Bright threads of light the waves focus on the ground under them.
float water_caustic(float2 p, float2 slope, float t) {
  float2 q = p * 3.0f + slope * 1.5f;
  float a = sin(q.x + 1.2f * sin(q.y * 1.3f + t * 0.9f) + t * 0.6f);
  float b = sin(q.y * 1.1f + 1.2f * sin(q.x * 0.9f - t * 0.7f) - t * 0.5f);
  return pow(saturate(1.0f - abs(a + b)), 4.0f);
}

// A slow churn that breaks foam up into lace.
float water_froth(float2 p, float t) {
  float a = sin(p.x * 9.0f + 1.5f * sin(p.y * 7.0f + t * 1.3f) + t * 0.9f);
  float b = sin(p.y * 11.0f + 1.5f * sin(p.x * 6.0f - t * 1.1f) - t * 0.7f);
  return saturate(0.5f + 0.5f * a * b);
}

// `mesh_falloff` and `mesh_band`, restated: a point light's reach, and
// one light flattened into `bands` tones.
float water_falloff(float distance, float range) {
  if (range <= 0.0f) {
    return 0.0f;
  }
  float reach = saturate(1.0f - distance / range);
  return reach * reach;
}

float water_band(float light, uint bands) {
  if (bands < 2u) {
    return light;
  }
  float top = float(bands - 1u);
  return min(floor(light * float(bands)), top) / top;
}

// What one light does to the water at p: the diffuse light its colour and
// foam take, in rgb, and the glint off the wave face, in w.
float4 water_light(MeshLight light, float3 p, float3 n, float3 v, uint bands) {
  float3 to_light = light.direction_intensity.xyz;
  float attenuation = 1.0f;
  if (light.color_kind.w == MESH_LIGHT_POINT) {
    float3 offset = light.position_range.xyz - p;
    attenuation = water_falloff(length(offset), light.position_range.w);
    to_light = offset;
  }
  float aim = length(to_light);
  if (aim < 1e-4f || attenuation <= 0.0f) {
    return float4(0.0f);
  }
  float3 l = to_light / aim;
  float strength = light.direction_intensity.w * attenuation;
  float ndh = saturate(dot(n, normalize(l + v)));
  float glint = pow(ndh, 60.0f) + 0.08f * pow(ndh, 12.0f);
  return float4(light.color_kind.xyz *
                    water_band(saturate(dot(n, l)) * attenuation, bands) *
                    light.direction_intensity.w * MESH_LIGHT_DIFFUSE,
                strength * glint);
}

// Premultiplied, over the terrain the ground drew under the water: the
// water's colour covers as much of it as its depth and opacity say, the
// sky covers more at a glance, and the light the waves focus on the
// ground, the foam and the glints are laid on top.
fragment float4 water_fs_main(WaterVsOut in [[stage_in]],
                              constant WaterShading& s [[buffer(0)]],
                              constant MeshLights& lights [[buffer(1)]],
                              texture2d<float> field [[texture(0)]]) {
  constexpr sampler smp(filter::linear, address::clamp_to_edge);
  float4 texel = field.sample(smp, in.uv);
  float level = (texel.b * 2.0f - 1.0f) * WATER_LEVEL_RANGE;
  // How deep the water is here: the tiles' depths blended between them,
  // shelving to nothing at the bank.
  float depth = in.depth * water_shelf(in.depth, texel.a * WATER_SHORE_TILES);
  float deepness = 1.0f - exp(-depth / max(s.clarity.z, 1e-3f));
  // Opacity sets how much a tile of water hides, evenly on a log scale
  // from the clearest to the murkiest; depth sets how many tiles there are.
  float absorb =
      s.clarity.x * pow(s.clarity.y / s.clarity.x, saturate(in.opacity));
  float cover = 1.0f - exp(-absorb * depth);
  float2 ripple =
      (texel.rg * 2.0f - 1.0f) * (WATER_SLOPE_RANGE * WATER_RIPPLE_GAIN);
  float t = s.view.w;
  // The shallows are sheltered: the wind raises less there, and a still
  // surface raises none.
  float2 slope = ripple + water_wind_slope(in.world.xy, t, s.detail.x) *
                              (s.detail.z * (0.3f + 0.7f * deepness));
  float3 n = normalize(float3(-slope, 1.0f));
  float3 v = s.view.xyz;
  // A ripple's face turned to the eye shows it more sky, and one turned
  // away less: linear in the slope, so a small ring reads as well as a
  // big one, where the Fresnel term alone would lose it.
  float fresnel =
      saturate(0.04f + 0.66f * pow(1.0f - saturate(dot(n, v)), 3.0f) +
               WATER_RIPPLE_SKY * dot(-ripple, v.xy));
  // Every light the meshes are lit by: its diffuse on the water, and its
  // glint off the wave faces in its own colour.
  float3 diffuse = float3(MESH_LIGHT_AMBIENT);
  float3 glint = float3(0.0f);
  for (uint i = 0; i < lights.count && i < MESH_MAX_LIGHTS; ++i) {
    float4 one = water_light(lights.lights[i], in.world, n, v,
                             lights.shade_bands);
    diffuse += one.rgb;
    glint += lights.lights[i].color_kind.xyz * one.w;
  }
  glint *= s.light.w;
  float3 water = water_linear(in.color * (1.0f - s.clarity.w * deepness)) *
                 diffuse * (1.0f + 2.0f * level);
  float skylight = min((diffuse.r + diffuse.g + diffuse.b) / 3.0f, 1.0f);
  float3 color = water * cover;
  float alpha = cover;
  color = color * (1.0f - fresnel) + water_linear(s.sky.rgb) * skylight * fresnel;
  alpha = alpha * (1.0f - fresnel) + fresnel;
  color += WATER_CAUSTIC_LIGHT * s.detail.y * (1.0f - alpha) * diffuse *
           water_caustic(in.world.xy, slope, t);
  float froth = water_froth(in.world.xy, t);
  float edge = 1.0f - smoothstep(0.05f, 0.28f, texel.a * WATER_SHORE_TILES);
  float crest =
      smoothstep(0.35f, 0.8f, level / WATER_LEVEL_RANGE) * s.foam.w;
  float foam = saturate(edge * (0.55f + 0.45f * froth) + crest * froth);
  color = color * (1.0f - foam) + water_linear(s.foam.rgb) * diffuse * foam;
  alpha = alpha * (1.0f - foam) + foam;
  float shine = max(glint.r, max(glint.g, glint.b));
  return float4(color + glint, saturate(alpha + shine));
}
)msl";

  bool buildWaterPipelinePso(id<MTLDevice> mtl_device,
                             id<MTLRenderPipelineState>* out_pso) {
    NSError* err = nil;
    NSString* src = [NSString stringWithUTF8String:WATER_MSL_SOURCE];
    id<MTLLibrary> lib = [mtl_device newLibraryWithSource:src
                                                  options:nil
                                                    error:&err];
    id<MTLFunction> vs = [lib newFunctionWithName:@"water_vs_main"];
    id<MTLFunction> fs = [lib newFunctionWithName:@"water_fs_main"];
    if (vs == nil || fs == nil) {
      return false;
    }
    auto* pd = [[MTLRenderPipelineDescriptor alloc] init];
    configureMeshRenderPipelineDesc(pd, vs, fs);
    // Premultiplied over the ground under it, in the pass that holds the
    // scene's depth: the water hides some of the ground and adds light to
    // the rest.
    applyPremultipliedBlend(pd.colorAttachments[0]);
    *out_pso = [mtl_device newRenderPipelineStateWithDescriptor:pd error:&err];
    (void)err;
    return *out_pso != nil;
  }

  /// The water's fixed state: tested against the scene's depth so a crate
  /// in a pond hides it, and not written, so the outline and the effects
  /// still see the bed.
  PipelineEntry waterPipelineEntry(id<MTLDevice> mtl_device,
                                   id<MTLRenderPipelineState> pso) {
    RhiDepthStencilState ds{};
    ds.depth_test = true;
    ds.depth_write = false;
    RhiRasterState raster{};
    raster.cull_back = false;
    return PipelineEntry{pso, nil, makeMtlDepthStencilState(mtl_device, ds),
                         RhiPrimitiveTopology::TRIANGLE_LIST, raster};
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

  bool createGuiRenderPipelineState(id<MTLDevice> mtl_device,
                                    id<MTLFunction> vs, id<MTLFunction> fs,
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
  bool tryCreateSkinnedMeshPipeline(RhiPipelineHandle& out) override;
  bool tryCreateMeshOutlinePipeline(RhiPipelineHandle& out) override;
  bool tryCreateFxParticlePipeline(RhiPipelineHandle& out) override;
  bool tryCreateFxVolumePipeline(RhiPipelineHandle& out) override;
  bool tryCreateWaterPipeline(RhiPipelineHandle& out) override;

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
  bool insertOutlinePipelineFromPso(id<MTLRenderPipelineState> pso,
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

    void setFragmentStageBytes(const void* data, size_t size,
                               uint32_t slot) override {
      if (render_enc_ == nil || data == nullptr || size == 0) {
        return;
      }
      [render_enc_ setFragmentBytes:data length:size atIndex:slot];
    }

    void bindFragmentTexture(RhiTextureHandle tex, uint32_t slot) override {
      if (render_enc_ == nil) {
        return;
      }
      [render_enc_ setFragmentTexture:device_->lookupTexture(tex) atIndex:slot];
    }

    void setViewport(const RhiViewport& vp) override {
      if (render_enc_ == nil) {
        return;
      }
      MTLViewport m{vp.x,      vp.y,         vp.width,
                    vp.height, vp.min_depth, vp.max_depth};
      [render_enc_ setViewport:m];
    }

    void setScissor(const RhiScissor& sc) override {
      if (render_enc_ == nil) {
        return;
      }
      MTLScissorRect r{static_cast<NSUInteger>(sc.x),
                       static_cast<NSUInteger>(sc.y), sc.width, sc.height};
      [render_enc_ setScissorRect:r];
    }

    void draw(const RhiDrawParams& p) override {
      if (render_enc_ == nil || bound_.render_pso == nil) {
        return;
      }
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
      // Metal has no first-index argument; the first index is a byte offset
      // into the index buffer. Dropping it drew every batch from index 0.
      const NSUInteger index_size = idx_type_ == MTLIndexTypeUInt32 ? 4 : 2;
      [render_enc_ drawIndexedPrimitives:toMtlPrimitiveType(bound_.topology)
                              indexCount:p.index_count
                               indexType:idx_type_
                             indexBuffer:idx_buf_
                       indexBufferOffset:idx_off_ + p.first_index * index_size
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
        if (src == nil || dst == nil) {
          return;
        }
        id<MTLBlitCommandEncoder> blit = [cmd_buffer_ blitCommandEncoder];
        [blit copyFromBuffer:src
                 sourceOffset:p.src_offset
                     toBuffer:dst
            destinationOffset:p.dst_offset
                         size:p.size];
        [blit endEncoding];
      }
    }

    void copyTextureToBuffer(RhiTextureHandle src_h,
                             RhiBufferHandle dst_h) override {
      @autoreleasepool {
        id<MTLTexture> tex = device_->lookupTexture(src_h);
        id<MTLBuffer> buf = device_->lookupBuffer(dst_h);
        if (tex == nil || buf == nil) {
          return;
        }
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
          ca.clearColor =
              MTLClearColorMake(info.clear_color[0], info.clear_color[1],
                                info.clear_color[2], info.clear_color[3]);
        }
      }
    }

    void configureDepthAttachment(MTLRenderPassDescriptor* rpd,
                                  const RhiRenderPassBeginInfo& info) {
      if (info.depth_target == RHI_TEXTURE_INVALID) {
        return;
      }
      rpd.depthAttachment.texture = device_->lookupTexture(info.depth_target);
      rpd.depthAttachment.loadAction = toMtlLoadAction(info.depth_load_op);
      rpd.depthAttachment.storeAction = MTLStoreActionStore;
      rpd.depthAttachment.clearDepth = info.clear_depth;
    }

    void applyBoundPipeline() {
      if (render_enc_ == nil || bound_.render_pso == nil) {
        return;
      }
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
                                    bound_.raster.front_ccw
                                        ? MtlFrontFaceWinding::COUNTER_CLOCKWISE
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
    if (device == nil) {
      return std::nullopt;
    }
    auto result = std::unique_ptr<MetalRealDevice>(new MetalRealDevice(config));
    if (!result->initGpuResources(device)) {
      return std::nullopt;
    }
    return std::unique_ptr<RhiDevice>(std::move(result));
  }
}

bool MetalRealDevice::initGpuResources(id<MTLDevice> device) {
  device_ = device;
  queue_ = [device newCommandQueue];
  unified_memory_ = [device hasUnifiedMemory];
  auto* window = static_cast<SDL_Window*>(config_.native_window);
  metal_view_ = SDL_Metal_CreateView(window);
  if (metal_view_ == nullptr) {
    return false;
  }
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
  layer.drawableSize =
      CGSizeMake(config_.backbuffer_width, config_.backbuffer_height);
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
    if (buf == nil) {
      return RHI_BUFFER_INVALID;
    }

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
    if (tex == nil) {
      return RHI_TEXTURE_INVALID;
    }
    if (desc.debug_name != nullptr) {
      tex.label = [NSString stringWithUTF8String:desc.debug_name];
    }
    auto h = next_handle_++;
    textures_.insert(h, tex);
    if (desc.initial_pixels != nullptr) {
      uploadInitialPixels(tex, desc);
    }
    return h;
  }
}

void MetalRealDevice::destroyTexture(RhiTextureHandle h) {
  textures_.erase(h);
}

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
    [tex replaceRegion:region
           mipmapLevel:0
             withBytes:u.pixels
           bytesPerRow:row_b];
    return true;
  }
}

// =========================================================================
// MetalRealDevice — Shader Lifecycle
// =========================================================================

ShaderEntry MetalRealDevice::compileShader(const RhiShaderDesc& desc) {
  auto data = dispatch_data_create(desc.bytecode, desc.bytecode_size, nullptr,
                                   DISPATCH_DATA_DESTRUCTOR_DEFAULT);
  NSError* err = nil;
  id<MTLLibrary> lib = [device_ newLibraryWithData:data error:&err];
  if (lib == nil) {
    return {};
  }
  auto* name = [NSString stringWithUTF8String:desc.entry_point];
  id<MTLFunction> func = [lib newFunctionWithName:name];
  if (func == nil) {
    return {};
  }
  return {lib, func};
}

RhiShaderHandle MetalRealDevice::createShader(const RhiShaderDesc& desc) {
  @autoreleasepool {
    auto entry = compileShader(desc);
    if (entry.function == nil) {
      return RHI_SHADER_INVALID;
    }
    auto h = next_handle_++;
    shaders_.insert(h, std::move(entry));
    return h;
  }
}

void MetalRealDevice::destroyShader(RhiShaderHandle h) {
  shaders_.erase(h);
}

// =========================================================================
// MetalRealDevice — Pipeline Lifecycle
// =========================================================================

RhiPipelineHandle
MetalRealDevice::createGraphicsPipeline(const RhiGraphicsPipelineDesc& desc) {
  @autoreleasepool {
    auto vs = shaders_.lookup(desc.vertex_shader);
    auto fs = shaders_.lookup(desc.fragment_shader);
    if (vs.function == nil) {
      return RHI_PIPELINE_INVALID;
    }

    auto* pd = buildPipelineDescriptor(vs, fs, desc);
    NSError* error = nil;
    auto pso = [device_ newRenderPipelineStateWithDescriptor:pd error:&error];
    if (pso == nil) {
      return RHI_PIPELINE_INVALID;
    }

    auto dss = makeMtlDepthStencilState(device_, desc.depth_stencil);
    auto h = next_handle_++;
    pipelines_.insert(h,
                      PipelineEntry{pso, nil, dss, desc.topology, desc.raster});
    return h;
  }
}

RhiPipelineHandle
MetalRealDevice::createComputePipeline(const RhiComputePipelineDesc& desc) {
  @autoreleasepool {
    auto cs = shaders_.lookup(desc.compute_shader);
    if (cs.function == nil) {
      return RHI_PIPELINE_INVALID;
    }

    NSError* error = nil;
    auto pso = [device_ newComputePipelineStateWithFunction:cs.function
                                                      error:&error];
    if (pso == nil) {
      return RHI_PIPELINE_INVALID;
    }

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
  pipelines_.insert(h,
                    PipelineEntry{pso, nil, dss,
                                  RhiPrimitiveTopology::TRIANGLE_LIST, raster});
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
  pipelines_.insert(
      h, PipelineEntry{pso, nil, makeMtlDepthStencilState(device_, ds),
                       RhiPrimitiveTopology::TRIANGLE_LIST, raster});
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

bool MetalRealDevice::tryCreateSkinnedMeshPipeline(RhiPipelineHandle& out) {
  @autoreleasepool {
    id<MTLRenderPipelineState> pso = nil;
    if (!buildSkinnedMeshPipelinePso(device_, &pso)) {
      return false;
    }
    // Same depth and raster state as static meshes: the two draw into one
    // pass and resolve against each other in its depth buffer.
    return insertMeshPipelineFromPso(pso, out);
  }
}

bool MetalRealDevice::insertOutlinePipelineFromPso(
    id<MTLRenderPipelineState> pso, RhiPipelineHandle& out) {
  // It reads the depth texture rather than testing against it: the pass it
  // draws in has no depth attachment, since this one is being sampled.
  RhiDepthStencilState ds{};
  ds.depth_test = false;
  ds.depth_write = false;
  RhiRasterState raster{};
  raster.cull_back = false;
  const auto h = next_handle_++;
  pipelines_.insert(
      h, PipelineEntry{pso, nil, makeMtlDepthStencilState(device_, ds),
                       RhiPrimitiveTopology::TRIANGLE_LIST, raster});
  out = h;
  return true;
}

bool MetalRealDevice::tryCreateMeshOutlinePipeline(RhiPipelineHandle& out) {
  @autoreleasepool {
    id<MTLRenderPipelineState> pso = nil;
    if (!buildOutlinePipelinePso(device_, &pso)) {
      return false;
    }
    return insertOutlinePipelineFromPso(pso, out);
  }
}

bool MetalRealDevice::tryCreateFxParticlePipeline(RhiPipelineHandle& out) {
  @autoreleasepool {
    id<MTLRenderPipelineState> pso = nil;
    if (!buildFxPipelinePso(device_, &pso)) {
      return false;
    }
    // The same fixed state as the outline's: the pass it draws in has no
    // depth attachment, since the scene's depth is the texture it reads.
    return insertOutlinePipelineFromPso(pso, out);
  }
}

bool MetalRealDevice::tryCreateFxVolumePipeline(RhiPipelineHandle& out) {
  @autoreleasepool {
    id<MTLRenderPipelineState> pso = nil;
    if (!buildFxVolumePipelinePso(device_, &pso)) {
      return false;
    }
    // Drawn in the same pass, under the same fixed state, as the particles.
    return insertOutlinePipelineFromPso(pso, out);
  }
}

bool MetalRealDevice::tryCreateWaterPipeline(RhiPipelineHandle& out) {
  @autoreleasepool {
    id<MTLRenderPipelineState> pso = nil;
    if (!buildWaterPipelinePso(device_, &pso)) {
      return false;
    }
    const auto h = next_handle_++;
    pipelines_.insert(h, waterPipelineEntry(device_, pso));
    out = h;
    return true;
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
  if (last_cmd_buffer_ == nil) {
    return;
  }
  __block dispatch_semaphore_t sem = frame_sem_;
  [last_cmd_buffer_ addCompletedHandler:^(id<MTLCommandBuffer>) {
    dispatch_semaphore_signal(sem);
  }];
}

void MetalRealDevice::submit(RhiCommandList&) {}

bool MetalRealDevice::present() {
  @autoreleasepool {
    if (drawable_ == nil || last_cmd_buffer_ == nil) {
      return false;
    }
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
    if (tex == nil) {
      return std::nullopt;
    }
    const uint32_t w = static_cast<uint32_t>([tex width]);
    const uint32_t h = static_cast<uint32_t>([tex height]);
    const auto sz = static_cast<size_t>(w) * h * BACKBUFFER_BYTES_PER_PIXEL;
    auto buf = [device_ newBufferWithLength:sz
                                    options:MTLResourceStorageModeShared];
    if (buf == nil) {
      return std::nullopt;
    }
    blitTextureToBuffer({tex, buf, w, h});
    return buildCaptureResult(buf, {tex, buf, w, h}, request);
  }
}

bool MetalRealDevice::captureToFile(const RhiCaptureRequest& request,
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
  if (result.has_value()) {
    return result;
  }
  return std::make_unique<MetalStubDevice>(config);
}

bool MetalRhiDevice::hasUnifiedMemory() const {
  if (!impl_) {
    return false;
  }
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
