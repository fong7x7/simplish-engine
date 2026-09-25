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
  constexpr uint32_t GUI_VERTEX_STRIDE = 72;

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
  uint color2 : ATTR3;
  float4 radii : ATTR4;
  float4 border : ATTR5;
  uint flags : ATTR6;
  float2 rect_wh : ATTR7;
  float param : ATTR8;
};

struct GuiVsOut {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
  float4 color : TEXCOORD1;
  nointerpolation float4 color2 : TEXCOORD2;
  nointerpolation uint flags : TEXCOORD3;
  nointerpolation float4 radii : TEXCOORD4;
  nointerpolation float4 border : TEXCOORD5;
  nointerpolation float2 rect_wh : TEXCOORD6;
  nointerpolation float param : TEXCOORD7;
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
  o.color2 = unpack_rgba8888(v.color2);
  o.flags = v.flags;
  o.radii = v.radii;
  o.border = v.border;
  o.rect_wh = v.rect_wh;
  o.param = v.param;
  return o;
}

float gui_sd_round_rect(float2 p, float2 half_ext, float4 radii) {
  float r = p.x < 0.0f ? (p.y < 0.0f ? radii.x : radii.w)
                       : (p.y < 0.0f ? radii.y : radii.z);
  r = clamp(r, 0.0f, min(half_ext.x, half_ext.y));
  float2 q = abs(p) - half_ext + float2(r, r);
  return length(max(q, float2(0.0f, 0.0f))) + min(max(q.x, q.y), 0.0f) - r;
}

float gui_cover(float d, float soft) {
  float w = max(max(fwidth(d), soft), 1e-4f);
  return 1.0f - smoothstep(-w, w, d);
}

float4 gui_fill(GuiVsOut i, float2 p) {
  if ((i.flags & 8u) != 0u) {
    float2 dir = float2(cos(i.param), sin(i.param));
    float len = abs(i.rect_wh.x * dir.x) + abs(i.rect_wh.y * dir.y);
    float t = dot(p, dir) / max(len, 1e-4f) + 0.5f;
    return lerp(i.color, i.color2, saturate(t));
  }
  if ((i.flags & 16u) != 0u) {
    float t = length(p / max(i.rect_wh * 0.5f, float2(1e-4f, 1e-4f)));
    return lerp(i.color, i.color2, saturate(t));
  }
  return i.color;
}

float gui_border_cover(float2 p, float2 half_ext, float4 radii, float4 b) {
  float outer_c = gui_cover(gui_sd_round_rect(p, half_ext, radii), 0.0f);
  float2 inner_half = half_ext - float2(b.w + b.y, b.x + b.z) * 0.5f;
  if (inner_half.x <= 0.0f || inner_half.y <= 0.0f) {
    return outer_c;
  }
  float2 centre = float2(b.w - b.y, b.x - b.z) * 0.5f;
  float4 inner_r = max(radii - float4(max(b.x, b.w), max(b.x, b.y),
                                      max(b.z, b.y), max(b.z, b.w)),
                       float4(0.0f, 0.0f, 0.0f, 0.0f));
  float inner_c =
      gui_cover(gui_sd_round_rect(p - centre, inner_half, inner_r), 0.0f);
  return outer_c * (1.0f - inner_c);
}

float4 gui_ps_main(GuiVsOut i) : SV_Target {
  if ((i.flags & 2u) != 0u) {
    return gui_texture.Sample(gui_sampler, i.uv) * i.color;
  }
  float2 half_ext = i.rect_wh * 0.5f;
  float2 p = (i.uv - float2(0.5f, 0.5f)) * i.rect_wh;
  float4 c = gui_fill(i, p);
  if ((i.flags & 32u) != 0u) {
    float2 shape = max(half_ext - float2(i.param, i.param), float2(0.0f, 0.0f));
    c.a *= gui_cover(gui_sd_round_rect(p, shape, i.radii), i.param * 0.5f);
  } else if (any(i.border > float4(1e-5f, 1e-5f, 1e-5f, 1e-5f))) {
    c.a *= gui_border_cover(p, half_ext, i.radii, i.border);
  } else if ((i.flags & 4u) != 0u) {
    c.a *= gui_cover(gui_sd_round_rect(p, half_ext, i.radii), 0.0f);
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
// The shared body below is written once, in types every backend reads,
// and spliced into each: these say what its words mean in HLSL.
#define WATER_CONST static const
#define mix lerp
#define fract frac
#define WATER_P
#define WATER_PC
#define WATER_A
#define WATER_AC
#define water_mul(m, v) mul(m, v)

struct WaterShading {
  float4x4 view_projection;
  float4 sky;
  float4 foam;
  float4 clarity;
  float4 absorb;
  float4 light;
  float4 view;
  float4 detail;
  float4 screen;
  float4 surface;
  float4 texel;
  float4 toggles;
};

cbuffer WaterShadingBuffer : register(b0) {
  WaterShading s;
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

Texture2D<float4> water_field : register(t0);
Texture2D<float4> water_scene : register(t1);
Texture2D<float> water_depth : register(t2);
Texture2D<float4> water_still : register(t3);
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

float4 water_field_at(float2 uv) { return water_field.Sample(water_sampler, uv); }

float4 water_still_at(float2 uv) { return water_still.Sample(water_sampler, uv); }

float3 water_scene_at(float2 uv) {
  return water_scene.Sample(water_sampler, uv).rgb;
}

float water_scene_depth(int2 p) {
  uint width;
  uint height;
  water_depth.GetDimensions(width, height);
  int2 top = int2(int(width), int(height)) - 1;
  return water_depth.Load(int3(clamp(p, int2(0, 0), top), 0));
}

uint water_light_count() { return water_light_header.x; }

uint water_shade_bands() { return water_light_header.y; }

float4 water_light_register(uint i, int k) {
  MeshLight light = water_lights[i];
  return k == 0 ? light.position_range
                : k == 1 ? light.direction_intensity : light.color_kind;
}

// Where a point in clip space lands on the copy of the scene: rows run
// down from the top.
float2 water_screen_uv(float4 clip) {
  float2 ndc = clip.xy / clip.w;
  float2 pixel = s.screen.xy + float2(0.5f + 0.5f * ndc.x, 0.5f - 0.5f * ndc.y) *
                                   s.surface.xy;
  return pixel * s.screen.zw;
}

// How deep a point in clip space lies, as the depth buffer holds it.
float water_clip_depth(float4 clip) { return clip.z / clip.w; }

// ---- The shared body: every backend's copy is this, word for word. ----
// `water-texels.h`'s and `water-field.h`'s ranges, restated.
WATER_CONST float WATER_SLOPE_RANGE = 1.0f;
WATER_CONST float WATER_LEVEL_RANGE = 0.1f;
WATER_CONST float WATER_SHORE_TILES = 2.0f;
WATER_CONST float WATER_WET_TILES = 0.35f;
// How much steeper the simulated ripples are drawn than they are: a ring a
// few hundredths of a tile high is what a wake is, and it has to read.
WATER_CONST float WATER_RIPPLE_GAIN = 3.0f;
// How much sky a ripple's slope towards the eye adds.
WATER_CONST float WATER_RIPPLE_SKY = 0.35f;
// How much the light the waves focus brightens the ground under them.
WATER_CONST float WATER_CAUSTIC_LIGHT = 0.6f;
// `water-depth.h`'s bank shelf, restated.
WATER_CONST float WATER_BANK_MIN_TILES = 0.3f;
WATER_CONST float WATER_BANK_TILES_PER_DEPTH = 0.5f;
WATER_CONST float WATER_BANK_MAX_TILES = 2.0f;
// How tight the glint off a smooth wave face is.
WATER_CONST float WATER_GLINT_SHARP = 600.0f;
// How rough even the smoothest water is, as a variance of its slope: the
// field's bytes cannot hold a slope finer than this.
WATER_CONST float WATER_BASE_ROUGHNESS = 0.002f;
// `mesh-light.h`'s, restated, as the mesh shader restates them.
WATER_CONST uint MESH_MAX_LIGHTS = 8u;
WATER_CONST float MESH_LIGHT_AMBIENT = 0.38f;
WATER_CONST float MESH_LIGHT_DIFFUSE = 0.62f;
WATER_CONST float MESH_LIGHT_POINT = 1.0f;

// Wind waves finer than the simulation: a direction, a wavelength in
// tiles, and a steepness — the slope at a crest. The first two are broad
// swell LOW and HIGH draw; the rest only HIGH does.
WATER_CONST int WATER_WAVE_COUNT = 6;

float4 water_wave(int i) {
  if (i == 0) {
    return float4(0.80f, 0.60f, 1.10f, 0.10f);
  }
  if (i == 1) {
    return float4(-0.45f, 0.89f, 0.63f, 0.08f);
  }
  if (i == 2) {
    return float4(0.97f, -0.24f, 0.39f, 0.06f);
  }
  if (i == 3) {
    return float4(0.20f, 0.98f, 0.25f, 0.05f);
  }
  if (i == 4) {
    return float4(-0.87f, 0.49f, 0.17f, 0.04f);
  }
  return float4(0.55f, -0.83f, 0.12f, 0.035f);
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

// How much of a pattern `size` tiles across a pixel `pixel` tiles wide
// can hold: all of it over four pixels, none under one and a half. What
// it cannot hold is left out rather than left to shimmer.
float water_resolved(float size, float pixel) {
  return smoothstep(1.5f, 4.0f, size / max(pixel, 1e-5f));
}

// The slope the wind waves add at p, t seconds in, in xy, and in z the
// slope left out of it as too fine for a pixel of `pixel` tiles, as a
// variance. Each travels at the speed deep water carries its wavelength,
// which grows as its root; each is peaked by `chop` — a crest sharper
// and a trough broader than a sine's, as a real wave's are — and all of
// them wander, their lines bent by a slow warp and their height by gusts,
// so a wide lake does not show the pattern repeating.
float3 water_wind_slope(float2 p, float t, float fine, float2 feel) {
  float pixel = feel.x;
  float chop = feel.y;
  float2 bent = p + 0.6f * float2(sin(p.y * 0.23f + t * 0.05f),
                                  sin(p.x * 0.19f - t * 0.04f));
  float gust = 0.7f + 0.3f * sin(dot(p, float2(0.13f, 0.21f)) + t * 0.3f) *
                          sin(dot(p, float2(-0.17f, 0.11f)) - t * 0.23f);
  float peak = 1.0f + 1.5f * chop;
  float2 slope = float2(0.0f, 0.0f);
  float lost = 0.0f;
  for (int i = 0; i < WATER_WAVE_COUNT; ++i) {
    float4 w = water_wave(i);
    float k = 6.2831853f / w.z;
    float phase = k * (dot(w.xy, bent) - 0.55f * sqrt(w.z) * t);
    float steep = (i < 2 ? 1.0f : fine) * w.w * gust;
    float held = water_resolved(w.z, pixel);
    float crest = peak * pow(0.5f + 0.5f * sin(phase), peak - 1.0f);
    slope += w.xy * (steep * held * crest * cos(phase));
    lost += 0.5f * steep * steep * (1.0f - held * held);
  }
  return float3(slope.x, slope.y, lost);
}

// Bright threads of light the waves focus on the ground under them.
// Their threads are a few tenths of a tile wide, and too far out to see
// they are their mean brightness instead.
float water_caustic(float2 p, float2 slope, float t, float pixel) {
  float2 q = p * 3.0f + slope * 1.5f;
  float a = sin(q.x + 1.2f * sin(q.y * 1.3f + t * 0.9f) + t * 0.6f);
  float b = sin(q.y * 1.1f + 1.2f * sin(q.x * 0.9f - t * 0.7f) - t * 0.5f);
  return mix(0.12f, pow(saturate(1.0f - abs(a + b)), 4.0f),
             water_resolved(0.3f, pixel));
}

// A slow churn that breaks foam up into lace.
float water_froth(float2 p, float t, float pixel) {
  float a = sin(p.x * 9.0f + 1.5f * sin(p.y * 7.0f + t * 1.3f) + t * 0.9f);
  float b = sin(p.y * 11.0f + 1.5f * sin(p.x * 6.0f - t * 1.1f) - t * 0.7f);
  return saturate(0.5f + 0.5f * a * b * water_resolved(0.35f, pixel));
}

// A value between 0 and 1 for the lattice point `cell`, the same every
// time it is asked.
float water_hash(float2 cell) {
  return fract(sin(dot(cell, float2(127.1f, 311.7f))) * 43758.5453f);
}

// Smooth noise between 0 and 1: the lattice's values eased between.
float water_noise(float2 p) {
  float2 cell = floor(p);
  float2 f = p - cell;
  float2 e = f * f * (3.0f - 2.0f * f);
  float a = water_hash(cell);
  float b = water_hash(cell + float2(1.0f, 0.0f));
  float c = water_hash(cell + float2(0.0f, 1.0f));
  float d = water_hash(cell + float2(1.0f, 1.0f));
  return mix(mix(a, b, e.x), mix(c, d, e.x), e.y);
}

// Bubbles in foam, drifting: two scales of noise, the finer one left out
// where a pixel cannot hold it.
float water_bubbles(float2 p, float t, float pixel) {
  float coarse = water_noise(p * 7.0f + float2(t * 0.31f, -t * 0.23f));
  float fine = water_noise(p * 19.0f - float2(t * 0.17f, t * 0.41f));
  return mix(coarse, 0.55f * coarse + 0.45f * fine,
             water_resolved(0.1f, pixel));
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

// One light's diffuse on the water at p, in rgb, and its glint off the
// wave face, in w. The light is its three registers: position and range,
// direction and intensity, colour and kind.
float4 water_light(float4 position_range, float4 direction_intensity,
                   float4 color_kind, float3 p, float3 n, float3 v,
                   uint bands, float sharp) {
  float3 to_light = direction_intensity.xyz;
  float attenuation = 1.0f;
  if (color_kind.w == MESH_LIGHT_POINT) {
    float3 offset = position_range.xyz - p;
    attenuation = water_falloff(length(offset), position_range.w);
    to_light = offset;
  }
  float aim = length(to_light);
  if (aim < 1e-4f || attenuation <= 0.0f) {
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
  }
  float3 l = to_light / aim;
  float strength = direction_intensity.w * attenuation;
  float ndh = saturate(dot(n, normalize(l + v)));
  // A narrow cone, as a light's reflection off water is — scattered
  // sparkles where a wave face catches it, not a sheet — widened and
  // dimmed as the surface a pixel covers roughens.
  float glint = pow(ndh, sharp) * (sharp + 8.0f) / (WATER_GLINT_SHARP + 8.0f);
  return float4(color_kind.xyz *
                    water_band(saturate(dot(n, l)) * attenuation, bands) *
                    direction_intensity.w * MESH_LIGHT_DIFFUSE,
                strength * glint);
}

// The waves lapping at the shore: how far apart they are and how often
// they come, how far out from the bank they rise, how high they are, and
// how far up the wet ground each runs, in tiles and seconds.
WATER_CONST float WATER_LAP_WAVELENGTH = 0.55f;
WATER_CONST float WATER_LAP_PERIOD = 2.6f;
WATER_CONST float WATER_LAP_REACH = 1.1f;
WATER_CONST float WATER_LAP_HEIGHT = 0.018f;
WATER_CONST float WATER_LAP_RUN = 0.16f;

// Where a lapping wave is at `p`, `shore` tiles out from the bank: the
// phase of the wave, rolling in towards the bank as time goes on, and
// arriving at different times along it so the shore is never in step.
float water_lap_phase(float2 p, float shore, float t) {
  float along = 1.8f * sin(p.x * 0.9f + 1.7f * sin(p.y * 0.7f)) +
                1.1f * sin(p.y * 1.3f - 0.8f * p.x);
  return 6.2831853f * (shore / WATER_LAP_WAVELENGTH + t / WATER_LAP_PERIOD) +
         along;
}

// How far up the wet ground the water has run at `p`, in tiles: out to
// `WATER_LAP_RUN` as a wave arrives, and back as it drains.
float water_lap_run(float2 p, float t) {
  float swell = 0.5f + 0.5f * sin(water_lap_phase(p, 0.0f, t));
  return WATER_LAP_RUN * swell * swell;
}

// How far from something standing in the water its foot is felt, how
// high up it the water looks for it, and how far above the water a
// surface has to be to count, in tiles.
WATER_CONST float WATER_CONTACT_TILES = 0.2f;
WATER_CONST float WATER_CONTACT_HEIGHT = 0.4f;
WATER_CONST float WATER_CONTACT_BIAS = 0.002f;
// How deep, in tiles, the water can be and still bend the ground under
// it further: deeper water hides the ground anyway.
WATER_CONST float WATER_BEND_DEPTH = 1.5f;

// How many steps a reflected ray is followed in, and how far behind a
// surface, in tiles, it may be and still be taken to have met it.
WATER_CONST int WATER_TRACE_STEPS = 24;
WATER_CONST float WATER_TRACE_THICKNESS = 0.6f;
// How much of the scene the water mirrors beyond what the Fresnel term
// says, so what stands over it shows in it from the camera's height.
WATER_CONST float WATER_MIRROR = 0.55f;

// `WATER_MAX_FLOW_SPEED`, restated: the flow the still texels hold is out
// of it.
WATER_CONST float WATER_FLOW_RANGE = 1.5f;
// How long, in seconds, the flowing surface drifts before each of its two
// layers jumps back: long enough not to be seen, short enough that nothing
// stretches.
WATER_CONST float WATER_DRIFT_CYCLE = 1.6f;

// How much of the wind's waves the thickest fluid still raises: most are
// gone, leaving a slow glossy swell.
WATER_CONST float WATER_VISCOUS_CALM = 0.85f;

// How much of the sky wet ground mirrors.
WATER_CONST float WATER_WET_SKY = 0.015f;
// How bright the lights' sheen on wet ground is, against a wave's glint.
WATER_CONST float WATER_WET_SHEEN = 0.35f;

// Where a point in the world lands: its place on the copy of the scene in
// xy, and its depth in z.
float3 water_project(WATER_PC float3 p) {
  float4 clip = water_mul(s.view_projection, float4(p.x, p.y, p.z, 1.0f));
  float2 at = water_screen_uv(WATER_AC clip);
  return float3(at.x, at.y, water_clip_depth(clip));
}

// The scene's depth at `at` on the copy of it.
float water_depth_at(WATER_PC float2 at) {
  return water_scene_depth(WATER_AC int2(at / s.screen.zw));
}

// How close the water at `world` is to the foot of something standing in
// it, from 1 against it to 0 `WATER_CONTACT_TILES` away. Looked for in
// eight directions on the water's own plane: a point there hidden by a
// surface only a little nearer the eye than it is hidden by the lower
// part of something that stands there, where the water meets it. `per_tile`
// is how much nearer the eye a tile of height brings a point, in depth.
float water_contact(WATER_PC float3 world, float per_tile) {
  float near = 0.0f;
  for (int i = 0; i < 8; ++i) {
    float a = 0.7853982f * float(i);
    float2 way = float2(cos(a), sin(a));
    for (int j = 1; j <= 2; ++j) {
      float reach = WATER_CONTACT_TILES * 0.5f * float(j);
      float3 at = water_project(WATER_AC float3(world.xy + way * reach,
                                                world.z));
      float above = (at.z - water_depth_at(WATER_AC at.xy)) / per_tile;
      if (above > WATER_CONTACT_BIAS && above < WATER_CONTACT_HEIGHT) {
        near = max(near, 1.0f - 0.5f * float(j - 1));
      }
    }
  }
  return near;
}

// How far along `ray` from `world` the ray has passed behind a surface of
// the scene, in tiles — positive once behind, measured along the eye's
// line.
float water_behind(WATER_PC float3 at, float per_tile) {
  float3 p = water_project(WATER_AC at);
  return (p.z - water_depth_at(WATER_AC p.xy)) / per_tile;
}

// What the water mirrors of the scene along `ray` from `world`: the scene
// where a surface stands in the ray's way within `s.surface.w` tiles, in
// rgb, and in w how much of it to take — none where nothing is met, and
// less towards the edge of the screen and the end of the ray's reach.
float4 water_reflect(WATER_PC float3 world, float3 ray, float per_tile) {
  float reach = s.surface.w;
  if (reach <= 0.0f || ray.z <= 0.0f) {
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
  }
  float pace = reach / float(WATER_TRACE_STEPS);
  float before = 0.0f;
  for (int i = 1; i <= WATER_TRACE_STEPS; ++i) {
    float along = pace * float(i);
    float behind = water_behind(WATER_AC world + ray * along, per_tile);
    if (behind > 0.0f && behind < WATER_TRACE_THICKNESS) {
      float lo = before;
      float hi = along;
      for (int j = 0; j < 4; ++j) {
        float mid = 0.5f * (lo + hi);
        if (water_behind(WATER_AC world + ray * mid, per_tile) > 0.0f) {
          hi = mid;
        } else {
          lo = mid;
        }
      }
      float3 hit = water_project(WATER_AC world + ray * hi);
      float2 edge = min(hit.xy, float2(1.0f, 1.0f) - hit.xy);
      float fade = smoothstep(0.0f, 0.06f, min(edge.x, edge.y)) *
                   (1.0f - smoothstep(0.6f * reach, reach, hi));
      return float4(water_scene_at(WATER_AC hit.xy), fade);
    }
    before = along;
  }
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

// The two places flowing water at `p` was carried from, in xy and zw:
// layers of the surface half a cycle apart, each drifting downstream with
// the flow and jumping back when its weight is nothing
// (`water_drift_weight`), so the surface runs without ever stretching.
float4 water_drift(float2 p, float2 flow, float t) {
  float a = fract(t / WATER_DRIFT_CYCLE) * WATER_DRIFT_CYCLE;
  float b = fract(t / WATER_DRIFT_CYCLE + 0.5f) * WATER_DRIFT_CYCLE;
  float2 first = p - flow * a;
  float2 second = p - flow * b;
  return float4(first.x, first.y, second.x, second.y);
}

// How much of the first layer `water_drift` gives to take: nothing as it
// jumps back, all of it half a cycle later.
float water_drift_weight(float t) {
  return 1.0f - abs(2.0f * fract(t / WATER_DRIFT_CYCLE) - 1.0f);
}

// Lines of foam drawn out along a current, drifting with it: long along
// the flow and thin across it, more of them the faster it runs.
float water_streaks(float2 p, float2 flow, float t, float pixel) {
  float speed = length(flow);
  if (speed < 0.05f) {
    return 0.0f;
  }
  float2 along = flow / speed;
  float2 q = float2(dot(p, along) * 1.3f - t * speed * 1.3f,
                    dot(p, float2(-along.y, along.x)) * 9.0f);
  float lane = smoothstep(0.78f, 0.97f, water_noise(q));
  return 0.6f * lane * saturate(speed / WATER_FLOW_RANGE * 1.5f) *
         water_resolved(0.12f, pixel);
}

// How far the water at a sample is from its shore, in tiles, from the still
// texels' R: above the middle, a wet sample's distance.
float water_shore_of(float4 still) {
  return max(still.r * 2.0f - 1.0f, 0.0f) * WATER_SHORE_TILES;
}

// How far dry land is from the water, in tiles, from the same channel:
// below the middle, a dry sample's distance.
float water_land_of(float4 still) {
  return max(1.0f - still.r * 2.0f, 0.0f) * WATER_WET_TILES;
}

// Which way the shore lies from `uv`, as the slope of its distance: a
// unit step away from the bank, or nothing out in the open.
float2 water_shore_way(WATER_PC float2 uv) {
  float2 du = float2(s.texel.x, 0.0f);
  float2 dv = float2(0.0f, s.texel.y);
  float2 rise = float2(water_still_at(WATER_AC uv + du).r -
                           water_still_at(WATER_AC uv - du).r,
                       water_still_at(WATER_AC uv + dv).r -
                           water_still_at(WATER_AC uv - dv).r);
  float size = length(rise);
  return size > 1e-4f ? rise / size : float2(0.0f, 0.0f);
}

// The ground the water has wet, at a fragment of the band around it:
// darker and richer — raised to a power, which darkens its dim channels
// more than its bright ones — and a little glossy, fading out
// `WATER_WET_TILES` from the water. Well inside the water, which is drawn over the band, nothing.
float4 water_wet_ground(WATER_PC float3 world, float4 still, float4 frag) {
  float wet = 1.0f - smoothstep(0.0f, 1.0f, water_land_of(still) /
                                                 WATER_WET_TILES);
  if (wet <= 0.0f || water_shore_of(still) > 0.25f) {
    discard;
  }
  float3 up = float3(0.0f, 0.0f, 1.0f);
  float3 sheen = float3(0.0f, 0.0f, 0.0f);
  uint count = water_light_count(WATER_A);
  for (uint i = 0u; i < count && i < MESH_MAX_LIGHTS; ++i) {
    float4 color_kind = water_light_register(WATER_AC i, 2);
    float4 one = water_light(water_light_register(WATER_AC i, 0),
                             water_light_register(WATER_AC i, 1), color_kind,
                             world, up, s.view.xyz, water_shade_bands(WATER_A),
                             WATER_GLINT_SHARP);
    sheen += color_kind.xyz * one.w;
  }
  float3 ground = water_scene_at(WATER_AC frag.xy * s.screen.zw);
  float soak = s.light.x * wet;
  float3 color = pow(ground, float3(1.0f + soak, 1.0f + soak, 1.0f + soak)) *
                     (1.0f - 0.5f * soak) +
                 (sheen * (WATER_WET_SHEEN * s.light.w) +
                  water_linear(s.sky.rgb) * WATER_WET_SKY) * wet;
  // The sheet of water a lapping wave runs up the ground: a glassy film,
  // with a line of foam at its edge.
  float land = water_land_of(still);
  float run = water_lap_run(world.xy, s.view.w) * s.light.y * (1.0f - still.a);
  float film = 1.0f - smoothstep(run - 0.03f, run, land);
  float lip = film * smoothstep(run - 0.05f, run - 0.01f, land) *
              (0.6f + 0.4f * water_froth(world.xy, s.view.w, s.texel.w));
  color = mix(color, color * 0.85f + (sheen * s.light.w +
                                      water_linear(s.sky.rgb) * 0.04f),
              0.6f * film);
  color = mix(color, water_linear(s.foam.rgb) * (0.4f + 0.6f * wet), lip);
  return float4(color, 1.0f);
}

// The water at one fragment: `frag` is where it lies on the target, in
// pixels, and its depth. Where something stands in front of the water,
// nothing; everywhere else the ground the copy of the scene holds, seen
// through the water — red lost first and blue last, so it fades through
// teal into the water's own colour — with the light the waves focus laid
// on it, the sky reflected over it at a glance, and the foam and the
// glints on top. Opaque, since the water has already been composed over
// the ground here.
float4 water_shade(WATER_PC float3 world, float2 uv, float depth_in,
                   float opacity, float3 tint, float4 frag) {
  if (water_scene_depth(WATER_AC int2(frag.xy)) < frag.z) {
    discard;
  }
  float4 still = water_still_at(WATER_AC uv);
  if (depth_in <= 0.0f) {
    return water_wet_ground(WATER_AC world, still, frag);
  }
  float4 texel = water_field_at(WATER_AC uv);
  // Which way the water runs here, and where the running surface was
  // carried from: still water was carried from nowhere but here.
  float2 flow = (float2(still.g, still.b) * 2.0f - 1.0f) *
                (WATER_FLOW_RANGE * s.detail.w);
  float4 from = water_drift(world.xy, flow, s.view.w);
  float drift = water_drift_weight(s.view.w);
  float level = (texel.b * 2.0f - 1.0f) * WATER_LEVEL_RANGE;
  float shore = water_shore_of(still);
  // How thick the water is: a thick fluid barely raises a wave or laps.
  float thick = still.a;
  // How deep the water is here: the tiles' depths blended between them,
  // shelving to nothing at the bank.
  float depth = depth_in * water_shelf(depth_in, shore);
  float deepness = 1.0f - exp(-depth / max(s.clarity.z, 1e-3f));
  // Opacity sets how much a tile of water absorbs, evenly on a log scale
  // from the clearest to the murkiest; depth sets how many tiles there
  // are; and each of red, green and blue goes at its own rate.
  float absorb = s.clarity.x * pow(s.clarity.y / s.clarity.x, saturate(opacity));
  float3 through = exp(-absorb * s.absorb.xyz * depth);
  // A pixel's width, in tiles: the finest detail worth drawing. Ripples
  // finer than the field's samples are left out.
  float pixel = s.texel.w;
  float2 ripple =
      (texel.rg * 2.0f - 1.0f) * (WATER_SLOPE_RANGE * WATER_RIPPLE_GAIN);
  float ripple_held = water_resolved(2.0f * s.texel.z, pixel);
  float rough = WATER_BASE_ROUGHNESS +
                dot(ripple, ripple) * (1.0f - ripple_held * ripple_held);
  ripple *= ripple_held;
  float t = s.view.w;
  // The waves lapping at the shore: rising out of the shallows, rolling
  // in across the way the shore lies, and breaking into foam at the bank.
  float lap = water_lap_phase(world.xy, shore, t);
  float near_bank = (1.0f - smoothstep(0.0f, WATER_LAP_REACH, shore)) *
                    s.light.y * (1.0f - thick);
  ripple += water_shore_way(WATER_AC uv) *
            (WATER_LAP_HEIGHT * 6.2831853f / WATER_LAP_WAVELENGTH *
             cos(lap) * near_bank *
             water_resolved(WATER_LAP_WAVELENGTH, pixel));
  // The shallows are sheltered: the wind raises less there, and a still
  // surface raises none.
  float calm = s.detail.z * (0.3f + 0.7f * deepness) *
               (1.0f - WATER_VISCOUS_CALM * thick);
  float3 wind = mix(water_wind_slope(from.zw, t, s.detail.x,
                                     float2(pixel, s.light.z)),
                    water_wind_slope(from.xy, t, s.detail.x,
                                     float2(pixel, s.light.z)),
                    drift);
  float2 slope = ripple + wind.xy * calm;
  rough += wind.z * calm * calm;
  // The rougher the surface a pixel covers, the wider and dimmer the
  // glint off it: detail too fine to draw still scatters the light.
  float sharp = WATER_GLINT_SHARP / (1.0f + 2.0f * WATER_GLINT_SHARP * rough);
  float3 n = normalize(float3(-slope.x, -slope.y, 1.0f));
  float3 v = s.view.xyz;
  // A ripple's face turned to the eye shows it more sky, and one turned
  // away less: linear in the slope, so a small ring reads as well as a
  // big one, where the Fresnel term alone would lose it.
  float fresnel =
      saturate(0.02f + 0.98f * pow(1.0f - saturate(dot(n, v)), 5.0f) +
               WATER_RIPPLE_SKY * dot(-ripple, v.xy));
  // Every light the meshes are lit by: its diffuse on the water, and its
  // glint off the wave faces in its own colour.
  float3 diffuse = float3(MESH_LIGHT_AMBIENT, MESH_LIGHT_AMBIENT,
                          MESH_LIGHT_AMBIENT);
  float3 glint = float3(0.0f, 0.0f, 0.0f);
  uint count = water_light_count(WATER_A);
  for (uint i = 0u; i < count && i < MESH_MAX_LIGHTS; ++i) {
    float4 color_kind = water_light_register(WATER_AC i, 2);
    float4 one = water_light(water_light_register(WATER_AC i, 0),
                             water_light_register(WATER_AC i, 1), color_kind,
                             world, n, v, water_shade_bands(WATER_A), sharp);
    diffuse += one.rgb;
    glint += color_kind.xyz * one.w;
  }
  glint *= s.light.w;
  float3 water = water_linear(tint * (1.0f - s.clarity.w * deepness)) *
                 diffuse * (1.0f + 2.0f * level);
  float skylight = min((diffuse.r + diffuse.g + diffuse.b) / 3.0f, 1.0f);
  // What is seen through the water, bent by its ripples: the ground a
  // little way along the slope, further under deeper water — unless what
  // lies there stands in front of the water, which it cannot show, and
  // the ground straight under it is seen instead.
  float2 here = frag.xy * s.screen.zw;
  float2 seen = here;
  if (s.absorb.w > 0.0f) {
    float3 bent = water_project(
        WATER_AC float3(world.xy - slope * (s.absorb.w *
                                            min(depth, WATER_BEND_DEPTH)),
                        world.z));
    seen = water_depth_at(WATER_AC bent.xy) >= bent.z ? bent.xy : here;
  }
  float3 ground = water_scene_at(WATER_AC seen);
  // A calm surface focuses little: thick fluid all but loses its caustics.
  ground *= 1.0f + WATER_CAUSTIC_LIGHT * s.detail.y *
                       (1.0f - WATER_VISCOUS_CALM * thick) *
                       mix(water_caustic(from.zw, slope, t, pixel),
                           water_caustic(from.xy, slope, t, pixel), drift);
  float3 color = ground * through + water * (1.0f - through);
  // Where something stands in the water: a shadow of it in the water
  // round its foot, and a ring of foam where the water meets it.
  float per_tile = water_project(WATER_AC world).z -
                   water_project(WATER_AC world + v).z;
  float contact = per_tile > 0.0f && s.toggles.x > 0.0f
                      ? water_contact(WATER_AC world, per_tile)
                      : 0.0f;
  color *= 1.0f - 0.25f * contact;
  color = mix(color, water_linear(s.sky.rgb) * skylight, fresnel);
  // And whatever stands over the water, mirrored in it, bent by its
  // waves as the sky is.
  float3 ray = reflect(-v, n);
  float4 mirrored = per_tile > 0.0f
                        ? water_reflect(WATER_AC world, ray, per_tile)
                        : float4(0.0f, 0.0f, 0.0f, 0.0f);
  color = mix(color, mirrored.rgb,
              mirrored.w * s.sky.w * saturate(fresnel + WATER_MIRROR));
  float froth = mix(water_froth(from.zw, t, pixel),
                    water_froth(from.xy, t, pixel), drift);
  float breaking = max(sin(lap), 0.0f) * s.light.y;
  float edge = 1.0f - smoothstep(0.03f, 0.12f + 0.2f * breaking, shore);
  float crest = smoothstep(0.35f, 0.8f, level / WATER_LEVEL_RANGE) * s.foam.w;
  float ring = contact * contact * (0.45f + 0.55f * froth);
  // Foam left by whatever churned the water, thinning into lace as it goes
  // rather than fading evenly: the less there is, the more of the churn
  // shows through it.
  float churned = texel.a;
  float gap = 1.0f - 0.75f * churned;
  float lace = 0.9f * min(1.0f, 1.2f * churned) *
               smoothstep(gap, gap + 0.15f,
                          mix(water_bubbles(from.zw, t, pixel),
                              water_bubbles(from.xy, t, pixel), drift));
  float foam = saturate(edge * (0.55f + 0.45f * froth) + crest * froth + ring +
                        lace + water_streaks(world.xy, flow, t, pixel));
  color = mix(color, water_linear(s.foam.rgb) * diffuse, foam);
  return float4(color + glint, 1.0f);
}
// ---- End of the shared body. ----

float4 water_ps_main(WaterVsOut i) : SV_Target {
  return water_shade(i.world, i.uv, i.depth, i.opacity, i.color, i.position);
}
)hlsl";

  /// Vertex input elements for `eng::GuiVertex`, in `buildInputLayout`'s
  /// "ATTR<location>" semantic convention.
  constexpr std::array<D3D12_INPUT_ELEMENT_DESC, 9> GUI_INPUT_ELEMENTS{{
      {"ATTR", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 8,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 2, DXGI_FORMAT_R32_UINT, 0, 16,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 3, DXGI_FORMAT_R32_UINT, 0, 20,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 4, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 5, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 40,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 6, DXGI_FORMAT_R32_UINT, 0, 56,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 7, DXGI_FORMAT_R32G32_FLOAT, 0, 60,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"ATTR", 8, DXGI_FORMAT_R32_FLOAT, 0, 68,
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
  pso.InputLayout = {MESH_INPUT_ELEMENTS.data(),
                     static_cast<UINT>(MESH_INPUT_ELEMENTS.size())};
  // Drawn in the pass over the scene, as the effects are: no depth
  // attachment, since the water reads the scene's depth itself.
  pso.DSVFormat = DXGI_FORMAT_UNKNOWN;
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
