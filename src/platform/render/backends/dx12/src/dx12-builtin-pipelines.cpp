#include "dx12-builtin-pipelines.h"

#ifdef ENGINE_RENDERER_DX12

#include <array>
#include <climits>
#include <cstring>
#include <d3dcompiler.h>

namespace eng::render {

namespace {

  /// Shader model the built-in shaders are compiled against. 5.1 is the
  /// highest FXC emits and is what every feature-level-12_0 device accepts.
  constexpr const char DX12_VS_TARGET[] = "vs_5_1";
  /// Pixel-stage counterpart of `DX12_VS_TARGET`.
  constexpr const char DX12_PS_TARGET[] = "ps_5_1";

  /// Byte stride of `eng::GuiVertex`, restated from `gui-vertex-layout.h`.
  constexpr uint32_t GUI_VERTEX_STRIDE = 40;

  /// Byte stride of `eng::MeshVertex`: two tightly packed float3s.
  constexpr uint32_t MESH_VERTEX_STRIDE = 32;

  /// HLSL for screen-space GUI quads. Mirrors `GUI_MSL_SOURCE`.
  constexpr const char GUI_HLSL_SOURCE[] = R"hlsl(
cbuffer GuiScreenToNdc : register(b1) {
  float2 gui_screen_scale;
};

Texture2D<float4> gui_texture : register(t0);
SamplerState gui_sampler : register(s0);

struct GuiVertexIn {
  float2 position : ATTR0;
  float2 uv : ATTR1;
  uint color : ATTR2;
  float corner_radius : ATTR3;
  float border_width : ATTR4;
  uint flags : ATTR5;
  float2 rect_wh : ATTR6;
};

struct GuiVsOut {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
  float4 color : TEXCOORD1;
  nointerpolation uint flags : TEXCOORD2;
  nointerpolation float corner_radius : TEXCOORD3;
  nointerpolation float border_width : TEXCOORD4;
  nointerpolation float2 rect_wh : TEXCOORD5;
};

// sRGB-encoded byte (0-1) -> linear, for output to an sRGB colour target.
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

GuiVsOut gui_vs_main(GuiVertexIn v) {
  GuiVsOut o;
  o.position = float4(v.position.x * gui_screen_scale.x - 1.0f,
                      1.0f - v.position.y * gui_screen_scale.y, 0.0f, 1.0f);
  o.uv = v.uv;
  o.color = unpack_rgba8888(v.color);
  o.flags = v.flags;
  o.corner_radius = v.corner_radius;
  o.border_width = v.border_width;
  o.rect_wh = v.rect_wh;
  return o;
}

float gui_rounded_shape_cover_from_p(float2 p, float2 rect_wh, float corner_r) {
  float rw = rect_wh.x;
  float rh = rect_wh.y;
  if (rw <= 0.f || rh <= 0.f) {
    return 0.f;
  }
  float r = min(max(corner_r, 0.f), min(rw, rh) * 0.5f);
  float2 half_ext = float2(rw, rh) * 0.5f;
  float2 b = max(half_ext - float2(r, r), float2(0.f, 0.f));
  float2 q = abs(p) - b;
  float d = length(max(q, float2(0.f, 0.f))) + min(max(q.x, q.y), 0.f) - r;
  float w = max(fwidth(d), 1e-4f);
  return 1.f - smoothstep(-w, w, d);
}

float4 gui_ps_main(GuiVsOut i) : SV_Target {
  if ((i.flags & 2u) != 0u) {
    return gui_texture.Sample(gui_sampler, i.uv) * i.color;
  }
  float2 p = float2((i.uv.x - 0.5f) * i.rect_wh.x,
                    (i.uv.y - 0.5f) * i.rect_wh.y);
  float4 c = i.color;
  const float bw = i.border_width;
  const uint rounded_flag = i.flags & 4u;
  if (bw > 1e-5f) {
    float cr_o = (rounded_flag != 0u) ? i.corner_radius : 0.f;
    float outer_c = gui_rounded_shape_cover_from_p(p, i.rect_wh, cr_o);
    float irw = max(i.rect_wh.x - 2.f * bw, 0.f);
    float irh = max(i.rect_wh.y - 2.f * bw, 0.f);
    float in_r = (rounded_flag != 0u) ? max(i.corner_radius - bw, 0.f) : 0.f;
    float inner_c = gui_rounded_shape_cover_from_p(p, float2(irw, irh), in_r);
    c.a *= outer_c * (1.f - inner_c);
    return c;
  }
  if (rounded_flag != 0u) {
    c.a *= gui_rounded_shape_cover_from_p(p, i.rect_wh, i.corner_radius);
    return c;
  }
  return c;
}
)hlsl";

  /// HLSL for static meshes. Mirrors `MESH_MSL_SOURCE`.
  ///
  /// The four constants below are `mesh-light.h`'s, restated because HLSL
  /// cannot include a C++ header. MESH_MAX_LIGHTS sizes the array, so it and
  /// the C++ one have to move together.
  constexpr const char MESH_HLSL_SOURCE[] = R"hlsl(
static const uint MESH_MAX_LIGHTS = 8;
static const float MESH_LIGHT_AMBIENT = 0.38f;
static const float MESH_LIGHT_DIFFUSE = 0.62f;
static const float MESH_LIGHT_POINT = 1.0f;

struct MeshLight {
  float4 position_range;
  float4 direction_intensity;
  float4 color_kind;
};

cbuffer MeshUniforms : register(b1) {
  float4x4 mesh_view_projection;
  float4x4 mesh_model;
};

// The header is the light count, then the band count, then padding.
cbuffer MeshLights : register(b0) {
  uint4 mesh_light_header;
  MeshLight mesh_lights[8];
};

Texture2D<float4> mesh_texture : register(t0);
// s1 wraps, so a tiling map tiles; s0 is the GUI's clamped one.
SamplerState mesh_sampler : register(s1);

struct MeshVertexIn {
  float3 position : ATTR0;
  float3 normal : ATTR1;
  float2 uv : ATTR2;
};

struct MeshVsOut {
  float4 position : SV_Position;
  float3 world_position : TEXCOORD0;
  float3 normal : TEXCOORD1;
  float2 uv : TEXCOORD2;
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
float mesh_falloff(float dist, float range) {
  if (range <= 0.0f) {
    return 0.0f;
  }
  float reach = saturate(1.0f - dist / range);
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
    return float3(0.0f, 0.0f, 0.0f);
  }
  float lambert = saturate(dot(normal, to_light / aim));
  return light.color_kind.xyz * light.direction_intensity.w *
         mesh_band(lambert * attenuation, bands) * MESH_LIGHT_DIFFUSE;
}

MeshVsOut mesh_vs_main(MeshVertexIn v) {
  MeshVsOut o;
  float4 world = mul(mesh_model, float4(v.position, 1.0f));
  o.position = mul(mesh_view_projection, world);
  o.world_position = world.xyz;
  // The placement transform is a rotation and a uniform scale, so the same
  // matrix carries the normal; the length the scale adds comes back out in
  // the normalize below.
  o.normal = mul(mesh_model, float4(v.normal, 0.0f)).xyz;
  o.uv = v.uv;
  return o;
}

float4 mesh_ps_main(MeshVsOut i) : SV_Target {
  float3 n = normalize(i.normal);
  float3 lit = float3(MESH_LIGHT_AMBIENT, MESH_LIGHT_AMBIENT,
                      MESH_LIGHT_AMBIENT);
  uint count = min(mesh_light_header.x, MESH_MAX_LIGHTS);
  for (uint k = 0; k < count; ++k) {
    lit += mesh_light_contribution(mesh_lights[k], i.world_position, n,
                                   mesh_light_header.y);
  }
  // The map is unorm, so this is the sRGB value the artist authored, shaded
  // and then converted on the way out. An instance with no map of its own
  // samples one texel of the flat colour this replaced, so there is no
  // untextured branch here.
  float3 base = saturate(mesh_texture.Sample(mesh_sampler, i.uv).rgb * lit);
  return float4(mesh_srgb_to_linear(base.r), mesh_srgb_to_linear(base.g),
                mesh_srgb_to_linear(base.b), 1.0f);
}
)hlsl";

  /// HLSL for the mesh outline. Mirrors `OUTLINE_MSL_SOURCE`; the cbuffer
  /// is `OutlineUniforms` in `mesh-outline-renderer.cpp`. The depth texture
  /// is the scene's own, read through an R32_FLOAT view of its typeless
  /// resource — see `dx12ResourceFormat`.
  constexpr const char OUTLINE_HLSL_SOURCE[] = R"hlsl(
cbuffer OutlineUniforms : register(b0) {
  float4 outline_color;
  float4 outline_bounds;
  float outline_width;
  float outline_threshold;
  float2 outline_pad;
};

Texture2D<float> outline_depth : register(t0);

float4 outline_vs_main(uint id : SV_VertexID) : SV_Position {
  // (-1,-1), (3,-1) and (-1,3): one triangle covering all of clip space,
  // so there is no seam down a diagonal and no vertex buffer to bind.
  float2 corner = float2(float((id << 1u) & 2u), float(id & 2u));
  return float4(corner * 2.0f - 1.0f, 0.0f, 1.0f);
}

/// Depth at a pixel, held inside the scissor so that nothing outside what
/// the scene drew into is read, and a mesh cut off by it is not lined.
float outline_depth_at(int2 p) {
  int2 lo = int2(outline_bounds.xy);
  int2 hi = int2(outline_bounds.zw) - 1;
  return outline_depth.Load(int3(clamp(p, lo, hi), 0));
}

float4 outline_ps_main(float4 position : SV_Position) : SV_Target {
  int2 p = int2(position.xy);
  float centre = outline_depth_at(p);
  // Nothing was drawn here, so there is nothing to outline.
  if (centre >= 1.0f) {
    discard;
  }
  int w = int(outline_width);
  float across = outline_depth_at(p + int2(w, 0)) +
                 outline_depth_at(p - int2(w, 0)) - 2.0f * centre;
  float down = outline_depth_at(p + int2(0, w)) +
               outline_depth_at(p - int2(0, w)) - 2.0f * centre;
  // Positive where this pixel is nearer than its neighbours on average,
  // which is the near side of an edge: the rim of the thing in front.
  float bend = max(across, down);
  if (bend <= outline_threshold) {
    discard;
  }
  float cover = saturate((bend - outline_threshold) /
                         max(outline_threshold, 1e-9f));
  return float4(outline_color.rgb, outline_color.a * cover);
}
)hlsl";

  /// Vertex input elements for `eng::GuiVertex`, in `buildInputLayout`'s
  /// "ATTR<location>" semantic convention.
  constexpr std::array<D3D12_INPUT_ELEMENT_DESC, 7> GUI_INPUT_ELEMENTS{{
      {"ATTR", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 8,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 2, DXGI_FORMAT_R32_UINT, 0, 16,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 3, DXGI_FORMAT_R32_FLOAT, 0, 20,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 4, DXGI_FORMAT_R32_FLOAT, 0, 24,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 5, DXGI_FORMAT_R32_UINT, 0, 28,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 6, DXGI_FORMAT_R32G32_FLOAT, 0, 32,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  }};

  /// Vertex input elements for `eng::MeshVertex`.
  constexpr std::array<D3D12_INPUT_ELEMENT_DESC, 3> MESH_INPUT_ELEMENTS{{
      {"ATTR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 2, DXGI_FORMAT_R32G32_FLOAT, 0, 24,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  }};

  /// A compiled vertex/pixel shader pair, owning both blobs.
  struct ShaderPair {
    /// Compiled vertex stage bytecode.
    ID3DBlob* vs = nullptr;
    /// Compiled pixel stage bytecode.
    ID3DBlob* ps = nullptr;
  };

  ID3DBlob* compileHlsl(const char* source, const char* entry,
                        const char* target) {
    ID3DBlob* code = nullptr;
    ID3DBlob* errors = nullptr;
    HRESULT hr = D3DCompile(source, std::strlen(source), nullptr, nullptr,
                            nullptr, entry, target, 0, 0, &code, &errors);
    if (errors != nullptr) {
      errors->Release();
    }
    if (FAILED(hr)) {
      return nullptr;
    }
    return code;
  }

  /// Compile both stages of one built-in shader; both blobs or neither.
  ShaderPair compilePair(const char* source, const char* vs_entry,
                         const char* ps_entry) {
    ShaderPair pair{};
    pair.vs = compileHlsl(source, vs_entry, DX12_VS_TARGET);
    pair.ps = compileHlsl(source, ps_entry, DX12_PS_TARGET);
    if (pair.vs != nullptr && pair.ps != nullptr) {
      return pair;
    }
    if (pair.vs != nullptr) {
      pair.vs->Release();
    }
    if (pair.ps != nullptr) {
      pair.ps->Release();
    }
    return {};
  }

  D3D12_SHADER_BYTECODE toBytecode(ID3DBlob* blob) {
    return {blob->GetBufferPointer(), blob->GetBufferSize()};
  }

  /// Premultiplied-friendly "over" compositing, matching the Metal GUI PSO.
  D3D12_BLEND_DESC buildGuiBlendDesc() {
    D3D12_BLEND_DESC desc{};
    auto& rt = desc.RenderTarget[0];
    rt.BlendEnable = TRUE;
    rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    rt.BlendOp = D3D12_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D12_BLEND_ONE;
    rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    return desc;
  }

  D3D12_BLEND_DESC buildOpaqueBlendDesc() {
    D3D12_BLEND_DESC desc{};
    desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    return desc;
  }

  /// OBJ files in the wild disagree about winding, and culling would drop
  /// half of some models entirely, so neither built-in pipeline culls.
  D3D12_RASTERIZER_DESC buildBuiltinRasterDesc() {
    D3D12_RASTERIZER_DESC desc{};
    desc.FillMode = D3D12_FILL_MODE_SOLID;
    desc.CullMode = D3D12_CULL_MODE_NONE;
    desc.DepthClipEnable = TRUE;
    return desc;
  }

  D3D12_DEPTH_STENCIL_DESC buildMeshDepthDesc() {
    D3D12_DEPTH_STENCIL_DESC desc{};
    desc.DepthEnable = TRUE;
    desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    return desc;
  }

  /// Fill the fields both built-in PSOs set the same way.
  void fillCommonPsoFields(D3D12_GRAPHICS_PIPELINE_STATE_DESC& pso,
                           const ShaderPair& shaders, DXGI_FORMAT color) {
    pso.VS = toBytecode(shaders.vs);
    pso.PS = toBytecode(shaders.ps);
    pso.RasterizerState = buildBuiltinRasterDesc();
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = color;
    pso.SampleDesc.Count = 1;
    pso.SampleMask = UINT_MAX;
  }

  ID3D12PipelineState* createPso(ID3D12Device5* device,
                                 const D3D12_GRAPHICS_PIPELINE_STATE_DESC& d) {
    ID3D12PipelineState* pso = nullptr;
    if (FAILED(device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&pso)))) {
      return nullptr;
    }
    return pso;
  }

  void releasePair(const ShaderPair& pair) {
    pair.vs->Release();
    pair.ps->Release();
  }

}  // namespace

ID3D12PipelineState* createDx12GuiPipelineState(ID3D12Device5* device,
                                                ID3D12RootSignature* root_sig,
                                                DXGI_FORMAT color_format) {
  const ShaderPair shaders =
      compilePair(GUI_HLSL_SOURCE, "gui_vs_main", "gui_ps_main");
  if (shaders.vs == nullptr) {
    return nullptr;
  }
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
  pso.pRootSignature = root_sig;
  fillCommonPsoFields(pso, shaders, color_format);
  pso.BlendState = buildGuiBlendDesc();
  pso.InputLayout = {GUI_INPUT_ELEMENTS.data(),
                     static_cast<UINT>(GUI_INPUT_ELEMENTS.size())};
  // No depth: the GUI is painted in draw order.
  pso.DSVFormat = DXGI_FORMAT_UNKNOWN;
  ID3D12PipelineState* state = createPso(device, pso);
  releasePair(shaders);
  return state;
}

ID3D12PipelineState* createDx12MeshPipelineState(ID3D12Device5* device,
                                                 ID3D12RootSignature* root_sig,
                                                 DXGI_FORMAT color_format) {
  const ShaderPair shaders =
      compilePair(MESH_HLSL_SOURCE, "mesh_vs_main", "mesh_ps_main");
  if (shaders.vs == nullptr) {
    return nullptr;
  }
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
  pso.pRootSignature = root_sig;
  fillCommonPsoFields(pso, shaders, color_format);
  pso.BlendState = buildOpaqueBlendDesc();
  pso.DepthStencilState = buildMeshDepthDesc();
  pso.InputLayout = {MESH_INPUT_ELEMENTS.data(),
                     static_cast<UINT>(MESH_INPUT_ELEMENTS.size())};
  pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
  ID3D12PipelineState* state = createPso(device, pso);
  releasePair(shaders);
  return state;
}

ID3D12PipelineState*
createDx12OutlinePipelineState(ID3D12Device5* device,
                               ID3D12RootSignature* root_sig,
                               DXGI_FORMAT color_format) {
  const ShaderPair shaders =
      compilePair(OUTLINE_HLSL_SOURCE, "outline_vs_main", "outline_ps_main");
  if (shaders.vs == nullptr) {
    return nullptr;
  }
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
  pso.pRootSignature = root_sig;
  fillCommonPsoFields(pso, shaders, color_format);
  // Blended like the GUI, so a softened crease pixel mixes with the scene.
  pso.BlendState = buildGuiBlendDesc();
  // No input layout: the triangle comes from SV_VertexID. No depth either,
  // since the depth is the texture being read.
  pso.DSVFormat = DXGI_FORMAT_UNKNOWN;
  ID3D12PipelineState* state = createPso(device, pso);
  releasePair(shaders);
  return state;
}

/// Byte stride the command list binds GUI vertex buffers with.
uint32_t dx12GuiVertexStride() {
  return GUI_VERTEX_STRIDE;
}

/// Byte stride the command list binds mesh vertex buffers with.
uint32_t dx12MeshVertexStride() {
  return MESH_VERTEX_STRIDE;
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_DX12
