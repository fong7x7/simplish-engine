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
  /// the C++ one have to move together; the fifth is
  /// `mesh-alpha-cutoff.h`'s.
  constexpr const char MESH_HLSL_SOURCE[] = R"hlsl(
static const uint MESH_MAX_LIGHTS = 8;
static const float MESH_LIGHT_AMBIENT = 0.38f;
static const float MESH_LIGHT_DIFFUSE = 0.62f;
static const float MESH_LIGHT_POINT = 1.0f;
static const float MESH_ALPHA_CUTOFF = 0.5f;

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

// Skinned meshes: a different vertex stage in front of the same pixel one.
// SKIN_MAX_JOINTS is `MESH_MAX_SKIN_JOINTS` in `skin-palette.h`, restated;
// the palette is `SkinPalette`, three rows of each joint's matrix.
static const uint SKIN_MAX_JOINTS = 80;

cbuffer SkinPalette : register(b2) {
  float4 skin_rows[240];
};

struct SkinnedVertexIn {
  float3 position : ATTR0;
  float3 normal : ATTR1;
  float2 uv : ATTR2;
  uint4 joints : ATTR3;
  float4 weights : ATTR4;
};

MeshVsOut skinned_vs_main(SkinnedVertexIn v) {
  // Linear blend skinning, a row at a time — `skinned_vs_main` in the MSL.
  float4 r0 = float4(0.0f, 0.0f, 0.0f, 0.0f);
  float4 r1 = r0;
  float4 r2 = r0;
  [unroll] for (uint k = 0; k < 4; ++k) {
    uint j = min(v.joints[k], SKIN_MAX_JOINTS - 1u) * 3u;
    r0 += skin_rows[j] * v.weights[k];
    r1 += skin_rows[j + 1u] * v.weights[k];
    r2 += skin_rows[j + 2u] * v.weights[k];
  }
  float4 p = float4(v.position, 1.0f);
  float4 n = float4(v.normal, 0.0f);
  MeshVsOut o;
  float4 world = mul(mesh_model, float4(dot(r0, p), dot(r1, p), dot(r2, p),
                                        1.0f));
  o.position = mul(mesh_view_projection, world);
  o.world_position = world.xyz;
  o.normal = mul(mesh_model, float4(dot(r0, n), dot(r1, n), dot(r2, n),
                                    0.0f)).xyz;
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
  float4 map = mesh_texture.Sample(mesh_sampler, i.uv);
  // Alpha-test cutout (ADR-003), which is what lets a sprite billboard go
  // through this pass: its empty corners have to not draw, and the pass is
  // opaque and depth-writing, so the only way for them not to is for their
  // fragments not to exist. Opaque geometry never reaches the branch.
  if (map.a < MESH_ALPHA_CUTOFF) {
    discard;
  }
  float3 base = saturate(map.rgb * lit);
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

  /// HLSL for effects particles. Mirrors `FX_MSL_SOURCE`: each vertex is
  /// `FxVertex`, already in clip space, and the cbuffer is `FxUniforms` in
  /// `fx-renderer.cpp`. The depth texture is the scene's, read to hide a
  /// particle behind geometry and fade it just in front.
  constexpr const char FX_HLSL_SOURCE[] = R"hlsl(
cbuffer FxUniforms : register(b0) {
  float fx_softness;
  float3 fx_pad;
};

Texture2D<float> fx_depth : register(t0);

struct FxVsIn {
  float4 clip : ATTR0;
  float4 color : ATTR1;
  float2 uv : ATTR2;
  float2 shape : ATTR3;
};

struct FxVsOut {
  float4 position : SV_Position;
  float4 color : COLOR0;
  float2 uv : TEXCOORD0;
  float2 shape : TEXCOORD1;
};

FxVsOut fx_vs_main(FxVsIn input) {
  FxVsOut output;
  output.position = input.clip;
  output.color = input.color;
  output.uv = input.uv;
  output.shape = input.shape;
  return output;
}

float fx_hash(float2 p) {
  return frac(sin(dot(p, float2(127.1f, 311.7f))) * 43758.5453f);
}

float fx_noise(float2 p) {
  float2 cell = floor(p);
  float2 f = frac(p);
  float2 s = f * f * (3.0f - 2.0f * f);
  float a = fx_hash(cell);
  float b = fx_hash(cell + float2(1.0f, 0.0f));
  float c = fx_hash(cell + float2(0.0f, 1.0f));
  float d = fx_hash(cell + float2(1.0f, 1.0f));
  return lerp(lerp(a, b, s.x), lerp(c, d, s.x), s.y);
}

float fx_fbm(float2 p) {
  return fx_noise(p) * 0.65f + fx_noise(p * 2.7f + 5.2f) * 0.35f;
}

// A disc broken up by noise: soft at the rim, uneven inside, and unlike the
// next particle's, because its seed moves the noise field under it.
float fx_puff(float2 uv, float seed) {
  float edge = saturate(1.0f - length(uv));
  float2 at = uv * 2.3f + float2(seed * 0.37f, seed * 0.71f);
  return saturate(edge * edge * (0.35f + 1.15f * fx_fbm(at)));
}

float4 fx_ps_main(FxVsOut input) : SV_Target {
  float scene = fx_depth.Load(int3(int2(input.position.xy), 0));
  float soft = saturate((scene - input.position.z) * fx_softness);
  float disc = saturate(1.0f - dot(input.uv, input.uv));
  float shape = lerp(disc * disc, fx_puff(input.uv, input.shape.y),
                     input.shape.x);
  float cover = shape * soft;
  if (cover <= 0.0f) {
    discard;
  }
  return input.color * cover;
}
)hlsl";

  /// HLSL for volumetric smoke. Mirrors `FX_VOLUME_MSL_SOURCE`: each
  /// vertex is `FxVolumeVertex`, already in clip space, carrying the ray
  /// its fragments march. There is no cbuffer; the depth texture is the
  /// scene's, read to stop the march where a surface is.
  constexpr const char FX_VOLUME_HLSL_SOURCE[] = R"hlsl(
Texture2D<float> fxv_depth : register(t0);

static const int FXV_STEPS = 16;

struct FxVolumeVsIn {
  float4 clip : ATTR0;
  float4 color : ATTR1;
  float4 origin : ATTR2;
  float4 ray : ATTR3;
  float4 params : ATTR4;
};

struct FxVolumeVsOut {
  float4 position : SV_Position;
  float4 color : COLOR0;
  float4 origin : TEXCOORD0;
  float4 ray : TEXCOORD1;
  float4 params : TEXCOORD2;
};

FxVolumeVsOut fx_volume_vs_main(FxVolumeVsIn input) {
  FxVolumeVsOut output;
  output.position = input.clip;
  output.color = input.color;
  output.origin = input.origin;
  output.ray = input.ray;
  output.params = input.params;
  return output;
}

float fxv_hash(float3 p) {
  return frac(sin(dot(p, float3(127.1f, 311.7f, 74.7f))) * 43758.5453f);
}

float fxv_noise(float3 p) {
  float3 cell = floor(p);
  float3 f = frac(p);
  float3 s = f * f * (3.0f - 2.0f * f);
  float x00 = lerp(fxv_hash(cell), fxv_hash(cell + float3(1.0f, 0.0f, 0.0f)),
                   s.x);
  float x10 = lerp(fxv_hash(cell + float3(0.0f, 1.0f, 0.0f)),
                   fxv_hash(cell + float3(1.0f, 1.0f, 0.0f)), s.x);
  float x01 = lerp(fxv_hash(cell + float3(0.0f, 0.0f, 1.0f)),
                   fxv_hash(cell + float3(1.0f, 0.0f, 1.0f)), s.x);
  float x11 = lerp(fxv_hash(cell + float3(0.0f, 1.0f, 1.0f)),
                   fxv_hash(cell + float3(1.0f, 1.0f, 1.0f)), s.x);
  return lerp(lerp(x00, x10, s.y), lerp(x01, x11, s.y), s.z);
}

float fxv_fbm(float3 p) {
  return fxv_noise(p) * 0.6f + fxv_noise(p * 2.3f + 11.0f) * 0.4f;
}

// How thick the smoke is at one point of the cloud's own space: an
// ellipsoid gone to nothing at the box's wall, eaten into by noise that the
// cloud's seed moves, so no two clouds are the same shape.
float fxv_density(float3 p, float seed) {
  float edge = saturate(1.0f - dot(p, p));
  float n = fxv_fbm(p * 1.9f + seed);
  return edge * edge * saturate(n * 1.7f - 0.45f);
}

float4 fx_volume_ps_main(FxVolumeVsOut input) : SV_Target {
  float3 o = input.origin.xyz;
  float3 d = input.ray.xyz;
  float3 inv = 1.0f / d;
  float3 near_wall = (float3(-1.0f, -1.0f, -1.0f) - o) * inv;
  float3 far_wall = (float3(1.0f, 1.0f, 1.0f) - o) * inv;
  float3 lo = min(near_wall, far_wall);
  float3 hi = max(near_wall, far_wall);
  float t_in = max(max(lo.x, lo.y), lo.z);
  // The scene stops the march where a surface is, so the smoke wraps what
  // it meets instead of cutting against it.
  float scene = fxv_depth.Load(int3(int2(input.position.xy), 0));
  float t_out = min(min(min(hi.x, hi.y), hi.z),
                    (scene - input.params.x) / input.ray.w);
  if (!(t_out > t_in)) {
    discard;
  }
  float dt = (t_out - t_in) / float(FXV_STEPS);
  float cover = 0.0f;
  float through = 1.0f;
  for (int i = 0; i < FXV_STEPS; ++i) {
    float3 p = o + d * (t_in + (float(i) + 0.5f) * dt);
    float a = 1.0f - exp(-fxv_density(p, input.origin.w) * input.params.y * dt);
    cover += through * a;
    through *= 1.0f - a;
  }
  if (cover <= 0.0f) {
    discard;
  }
  return input.color * cover;
}
)hlsl";

  /// HLSL for the water surface's vertex stage. Mirrors `water_vs_main`;
  /// the cbuffer is `water-vertex-uniforms.h`. Its own source, apart from
  /// the pixel stage's, because the two stages each read a `b1` of their
  /// own: this one the matrix, that one the scene's lights.
  constexpr const char WATER_VS_HLSL_SOURCE[] = R"hlsl(
cbuffer WaterUniforms : register(b1) {
  float4x4 water_view_projection;
  float4 water_field;
};

// A surface vertex: where it is, the water's colour in the normal's place,
// and its depth and opacity in the texture coordinate's.
struct WaterVertexIn {
  float3 position : ATTR0;
  float3 normal : ATTR1;
  float2 uv : ATTR2;
};

struct WaterVsOut {
  float4 position : SV_Position;
  float3 world : TEXCOORD0;
  float2 uv : TEXCOORD1;
  float depth : TEXCOORD2;
  float opacity : TEXCOORD3;
  float3 color : TEXCOORD4;
};

WaterVsOut water_vs_main(WaterVertexIn v) {
  WaterVsOut o;
  o.position = mul(water_view_projection, float4(v.position, 1.0f));
  o.world = v.position;
  o.uv = (v.position.xy - water_field.xy) * water_field.zw;
  o.depth = v.uv.x;
  o.opacity = v.uv.y;
  o.color = v.normal;
  return o;
}
)hlsl";

  /// HLSL for the water surface's pixel stage. Mirrors `water_fs_main`
  /// line for line; the cbuffers are `water-shading.h` at `b0` and the
  /// scene's lights at `b1`, in `MESH_HLSL_SOURCE`'s own layout.
  constexpr const char WATER_PS_HLSL_SOURCE[] = R"hlsl(
static const float WATER_SLOPE_RANGE = 1.0f;
static const float WATER_LEVEL_RANGE = 0.1f;
static const float WATER_SHORE_TILES = 2.0f;
static const float WATER_RIPPLE_GAIN = 3.0f;
static const float WATER_RIPPLE_SKY = 1.2f;
static const float WATER_CAUSTIC_LIGHT = 0.25f;
static const float WATER_BANK_MIN_TILES = 0.3f;
static const float WATER_BANK_TILES_PER_DEPTH = 0.5f;
static const float WATER_BANK_MAX_TILES = 2.0f;
static const uint MESH_MAX_LIGHTS = 8;
static const float MESH_LIGHT_AMBIENT = 0.38f;
static const float MESH_LIGHT_DIFFUSE = 0.62f;
static const float MESH_LIGHT_POINT = 1.0f;

static const float4 WATER_WAVES[4] = {
    float4(0.80f, 0.60f, 1.10f, 0.10f), float4(-0.45f, 0.89f, 0.63f, 0.08f),
    float4(0.97f, -0.24f, 0.39f, 0.06f), float4(0.20f, 0.98f, 0.25f, 0.05f)};

cbuffer WaterShading : register(b0) {
  float4 water_sky;
  float4 water_foam;
  float4 water_clarity;
  float4 water_light;
  float4 water_view;
  float4 water_detail;
};

struct MeshLight {
  float4 position_range;
  float4 direction_intensity;
  float4 color_kind;
};

// The header is the light count, then the band count, then padding.
cbuffer WaterLights : register(b1) {
  uint4 water_light_header;
  MeshLight water_lights[8];
};

Texture2D<float4> water_texture : register(t0);
// s0 clamps: the field ends at the water's dry ring.
SamplerState water_sampler : register(s0);

struct WaterVsOut {
  float4 position : SV_Position;
  float3 world : TEXCOORD0;
  float2 uv : TEXCOORD1;
  float depth : TEXCOORD2;
  float opacity : TEXCOORD3;
  float3 color : TEXCOORD4;
};

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

float water_shelf(float depth, float shore) {
  float run = clamp(WATER_BANK_MIN_TILES + WATER_BANK_TILES_PER_DEPTH * depth,
                    WATER_BANK_MIN_TILES, WATER_BANK_MAX_TILES);
  return smoothstep(0.0f, 1.0f, saturate(shore / run));
}

float2 water_wind_slope(float2 p, float t, float fine) {
  float2 slope = float2(0.0f, 0.0f);
  for (int i = 0; i < 4; ++i) {
    float4 w = WATER_WAVES[i];
    float k = 6.2831853f / w.z;
    float phase = k * (dot(w.xy, p) - 0.55f * sqrt(w.z) * t);
    float weight = i < 2 ? 1.0f : fine;
    slope += w.xy * (w.w * weight * cos(phase));
  }
  return slope;
}

float water_caustic(float2 p, float2 slope, float t) {
  float2 q = p * 3.0f + slope * 1.5f;
  float a = sin(q.x + 1.2f * sin(q.y * 1.3f + t * 0.9f) + t * 0.6f);
  float b = sin(q.y * 1.1f + 1.2f * sin(q.x * 0.9f - t * 0.7f) - t * 0.5f);
  return pow(saturate(1.0f - abs(a + b)), 4.0f);
}

float water_froth(float2 p, float t) {
  float a = sin(p.x * 9.0f + 1.5f * sin(p.y * 7.0f + t * 1.3f) + t * 0.9f);
  float b = sin(p.y * 11.0f + 1.5f * sin(p.x * 6.0f - t * 1.1f) - t * 0.7f);
  return saturate(0.5f + 0.5f * a * b);
}

float water_falloff(float dist, float range) {
  if (range <= 0.0f) {
    return 0.0f;
  }
  float reach = saturate(1.0f - dist / range);
  return reach * reach;
}

float water_band(float light, uint bands) {
  if (bands < 2u) {
    return light;
  }
  float top = float(bands - 1u);
  return min(floor(light * float(bands)), top) / top;
}

float4 water_one_light(MeshLight light, float3 p, float3 n, float3 v,
                       uint bands) {
  float3 to_light = light.direction_intensity.xyz;
  float attenuation = 1.0f;
  if (light.color_kind.w == MESH_LIGHT_POINT) {
    float3 offset = light.position_range.xyz - p;
    attenuation = water_falloff(length(offset), light.position_range.w);
    to_light = offset;
  }
  float aim = length(to_light);
  if (aim < 1e-4f || attenuation <= 0.0f) {
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
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

float4 water_ps_main(WaterVsOut i) : SV_Target {
  float4 texel = water_texture.Sample(water_sampler, i.uv);
  float level = (texel.b * 2.0f - 1.0f) * WATER_LEVEL_RANGE;
  float depth = i.depth * water_shelf(i.depth, texel.a * WATER_SHORE_TILES);
  float deepness = 1.0f - exp(-depth / max(water_clarity.z, 1e-3f));
  float absorb = water_clarity.x *
                 pow(water_clarity.y / water_clarity.x, saturate(i.opacity));
  float cover = 1.0f - exp(-absorb * depth);
  float2 ripple =
      (texel.rg * 2.0f - 1.0f) * (WATER_SLOPE_RANGE * WATER_RIPPLE_GAIN);
  float t = water_view.w;
  float2 slope = ripple + water_wind_slope(i.world.xy, t, water_detail.x) *
                              (water_detail.z * (0.3f + 0.7f * deepness));
  float3 n = normalize(float3(-slope, 1.0f));
  float3 v = water_view.xyz;
  float fresnel =
      saturate(0.04f + 0.66f * pow(1.0f - saturate(dot(n, v)), 3.0f) +
               WATER_RIPPLE_SKY * dot(-ripple, v.xy));
  float3 diffuse = float3(MESH_LIGHT_AMBIENT, MESH_LIGHT_AMBIENT,
                          MESH_LIGHT_AMBIENT);
  float3 glint = float3(0.0f, 0.0f, 0.0f);
  uint count = min(water_light_header.x, MESH_MAX_LIGHTS);
  for (uint k = 0; k < count; ++k) {
    float4 one = water_one_light(water_lights[k], i.world, n, v,
                                 water_light_header.y);
    diffuse += one.rgb;
    glint += water_lights[k].color_kind.xyz * one.w;
  }
  glint *= water_light.w;
  float3 water = water_linear(i.color * (1.0f - water_clarity.w * deepness)) *
                 diffuse * (1.0f + 2.0f * level);
  float skylight = min((diffuse.r + diffuse.g + diffuse.b) / 3.0f, 1.0f);
  float3 color = water * cover;
  float alpha = cover;
  color = color * (1.0f - fresnel) +
          water_linear(water_sky.rgb) * skylight * fresnel;
  alpha = alpha * (1.0f - fresnel) + fresnel;
  color += WATER_CAUSTIC_LIGHT * water_detail.y * (1.0f - alpha) * diffuse *
           water_caustic(i.world.xy, slope, t);
  float froth = water_froth(i.world.xy, t);
  float edge = 1.0f - smoothstep(0.05f, 0.28f, texel.a * WATER_SHORE_TILES);
  float crest =
      smoothstep(0.35f, 0.8f, level / WATER_LEVEL_RANGE) * water_foam.w;
  float foam = saturate(edge * (0.55f + 0.45f * froth) + crest * froth);
  color = color * (1.0f - foam) + water_linear(water_foam.rgb) * diffuse * foam;
  alpha = alpha * (1.0f - foam) + foam;
  float shine = max(glint.r, max(glint.g, glint.b));
  return float4(color + glint, saturate(alpha + shine));
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

  /// Byte stride of `eng::SkinnedMeshVertex`, and where its joints and
  /// weights sit — `skinned-mesh-vertex.h` asserts the same three numbers.
  constexpr uint32_t SKINNED_VERTEX_STRIDE = 52;

  /// Vertex input elements for `eng::SkinnedMeshVertex`: the static mesh's
  /// three, then four joint bytes read as integers and four weights.
  constexpr std::array<D3D12_INPUT_ELEMENT_DESC, 5> SKINNED_INPUT_ELEMENTS{{
      {"ATTR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 2, DXGI_FORMAT_R32G32_FLOAT, 0, 24,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 3, DXGI_FORMAT_R8G8B8A8_UINT, 0, 32,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 4, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 36,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  }};

  /// Byte stride of `eng::FxVertex`, as `fx-vertex.h` asserts.
  constexpr uint32_t FX_VERTEX_STRIDE = 48;

  /// Vertex input elements for `eng::FxVertex`: clip position, colour, uv,
  /// and the shape pair.
  constexpr std::array<D3D12_INPUT_ELEMENT_DESC, 4> FX_INPUT_ELEMENTS{{
      {"ATTR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 2, DXGI_FORMAT_R32G32_FLOAT, 0, 32,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 3, DXGI_FORMAT_R32G32_FLOAT, 0, 40,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  }};

  /// Byte stride of `eng::FxVolumeVertex`, as `fx-volume-vertex.h` asserts.
  constexpr uint32_t FX_VOLUME_STRIDE = 80;

  /// Vertex input elements for `eng::FxVolumeVertex`: five float4s.
  constexpr std::array<D3D12_INPUT_ELEMENT_DESC, 5> FX_VOLUME_INPUT_ELEMENTS{{
      {"ATTR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 4, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 64,
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

  /// @p pair when both its stages compiled; otherwise neither, with the one
  /// that did released.
  ShaderPair bothOrNeither(ShaderPair pair) {
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

  /// Compile both stages of one built-in shader; both blobs or neither.
  ShaderPair compilePair(const char* source, const char* vs_entry,
                         const char* ps_entry) {
    return bothOrNeither({compileHlsl(source, vs_entry, DX12_VS_TARGET),
                          compileHlsl(source, ps_entry, DX12_PS_TARGET)});
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

  /// Premultiplied compositing for effects: the colour added as it is, and
  /// the target kept by what the particle does not hide.
  D3D12_BLEND_DESC buildPremultipliedBlendDesc() {
    D3D12_BLEND_DESC desc = buildGuiBlendDesc();
    desc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
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

  /// Compile the water's two stages, each from its own source; both
  /// blobs or neither.
  ShaderPair compileWaterPair() {
    return bothOrNeither(
        {compileHlsl(WATER_VS_HLSL_SOURCE, "water_vs_main", DX12_VS_TARGET),
         compileHlsl(WATER_PS_HLSL_SOURCE, "water_ps_main", DX12_PS_TARGET)});
  }

  /// The water's depth: tested, so what stands in front of it hides it,
  /// and not written, so what is drawn after still sees the bed.
  D3D12_DEPTH_STENCIL_DESC buildWaterDepthDesc() {
    D3D12_DEPTH_STENCIL_DESC desc = buildMeshDepthDesc();
    desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
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
createDx12SkinnedMeshPipelineState(ID3D12Device5* device,
                                   ID3D12RootSignature* root_sig,
                                   DXGI_FORMAT color_format) {
  const ShaderPair shaders =
      compilePair(MESH_HLSL_SOURCE, "skinned_vs_main", "mesh_ps_main");
  if (shaders.vs == nullptr) {
    return nullptr;
  }
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
  pso.pRootSignature = root_sig;
  fillCommonPsoFields(pso, shaders, color_format);
  pso.BlendState = buildOpaqueBlendDesc();
  pso.DepthStencilState = buildMeshDepthDesc();
  pso.InputLayout = {SKINNED_INPUT_ELEMENTS.data(),
                     static_cast<UINT>(SKINNED_INPUT_ELEMENTS.size())};
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

ID3D12PipelineState* createDx12FxPipelineState(ID3D12Device5* device,
                                               ID3D12RootSignature* root_sig,
                                               DXGI_FORMAT color_format) {
  const ShaderPair shaders =
      compilePair(FX_HLSL_SOURCE, "fx_vs_main", "fx_ps_main");
  if (shaders.vs == nullptr) {
    return nullptr;
  }
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
  pso.pRootSignature = root_sig;
  fillCommonPsoFields(pso, shaders, color_format);
  pso.BlendState = buildPremultipliedBlendDesc();
  pso.InputLayout = {FX_INPUT_ELEMENTS.data(),
                     static_cast<UINT>(FX_INPUT_ELEMENTS.size())};
  // No depth attachment: like the outline, it reads the scene's depth.
  pso.DSVFormat = DXGI_FORMAT_UNKNOWN;
  ID3D12PipelineState* state = createPso(device, pso);
  releasePair(shaders);
  return state;
}

ID3D12PipelineState*
createDx12FxVolumePipelineState(ID3D12Device5* device,
                                ID3D12RootSignature* root_sig,
                                DXGI_FORMAT color_format) {
  const ShaderPair shaders = compilePair(
      FX_VOLUME_HLSL_SOURCE, "fx_volume_vs_main", "fx_volume_ps_main");
  if (shaders.vs == nullptr) {
    return nullptr;
  }
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
  pso.pRootSignature = root_sig;
  fillCommonPsoFields(pso, shaders, color_format);
  pso.BlendState = buildPremultipliedBlendDesc();
  pso.InputLayout = {FX_VOLUME_INPUT_ELEMENTS.data(),
                     static_cast<UINT>(FX_VOLUME_INPUT_ELEMENTS.size())};
  // The same pass as the particles: no depth attachment, since the depth
  // is the texture being read.
  pso.DSVFormat = DXGI_FORMAT_UNKNOWN;
  ID3D12PipelineState* state = createPso(device, pso);
  releasePair(shaders);
  return state;
}

ID3D12PipelineState* createDx12WaterPipelineState(ID3D12Device5* device,
                                                  ID3D12RootSignature* root_sig,
                                                  DXGI_FORMAT color_format) {
  const ShaderPair shaders = compileWaterPair();
  if (shaders.vs == nullptr) {
    return nullptr;
  }
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
  pso.pRootSignature = root_sig;
  fillCommonPsoFields(pso, shaders, color_format);
  pso.BlendState = buildPremultipliedBlendDesc();
  pso.DepthStencilState = buildWaterDepthDesc();
  pso.InputLayout = {MESH_INPUT_ELEMENTS.data(),
                     static_cast<UINT>(MESH_INPUT_ELEMENTS.size())};
  pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
  ID3D12PipelineState* state = createPso(device, pso);
  releasePair(shaders);
  return state;
}

/// Byte stride the command list binds effects vertex buffers with.
uint32_t dx12FxVertexStride() {
  return FX_VERTEX_STRIDE;
}

/// Byte stride the command list binds volume vertex buffers with.
uint32_t dx12FxVolumeVertexStride() {
  return FX_VOLUME_STRIDE;
}

/// Byte stride the command list binds GUI vertex buffers with.
uint32_t dx12GuiVertexStride() {
  return GUI_VERTEX_STRIDE;
}

/// Byte stride the command list binds mesh vertex buffers with.
uint32_t dx12MeshVertexStride() {
  return MESH_VERTEX_STRIDE;
}

/// Byte stride the command list binds skinned mesh vertex buffers with.
uint32_t dx12SkinnedMeshVertexStride() {
  return SKINNED_VERTEX_STRIDE;
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_DX12
