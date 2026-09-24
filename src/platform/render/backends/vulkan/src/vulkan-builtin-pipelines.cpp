#include "vulkan-builtin-pipelines.h"

#ifdef ENGINE_RENDERER_VULKAN

#include "vulkan-glsl-compiler.h"

#include <array>
#include <cstdint>
#include <vector>

namespace eng::render {

namespace {

  // -------------------------------------------------------------------------
  // GLSL. Set 0's bindings are the ones `vulkan-shared-layout.h` numbers:
  // vertex slot N is binding N, fragment slot N is binding 3 + N, the
  // texture is 5, and the clamping and repeating samplers are 6 and 7.
  // -------------------------------------------------------------------------

  /// GLSL for screen-space GUI quads' vertex stage. Mirrors `gui_vs_main`.
  constexpr const char GUI_VERT_GLSL[] = R"glsl(
#version 450

layout(set = 0, binding = 1, std140) uniform ScreenToNdc {
  vec2 scale;
} screen;

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in uint in_color;
layout(location = 3) in float in_corner_radius;
layout(location = 4) in float in_border_width;
layout(location = 5) in uint in_flags;
layout(location = 6) in vec2 in_rect_wh;

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec4 out_color;
layout(location = 2) flat out uint out_flags;
layout(location = 3) flat out float out_corner_radius;
layout(location = 4) flat out float out_border_width;
layout(location = 5) flat out vec2 out_rect_wh;

// sRGB-encoded byte (0-1) -> linear, for output to an sRGB colour target.
float srgb_byte_to_linear(float srgb) {
  if (srgb <= 0.04045) {
    return srgb / 12.92;
  }
  return pow((srgb + 0.055) / 1.055, 2.4);
}

vec4 unpack_rgba8888(uint c) {
  float r = float((c >> 0u) & 255u) / 255.0;
  float g = float((c >> 8u) & 255u) / 255.0;
  float b = float((c >> 16u) & 255u) / 255.0;
  float a = float((c >> 24u) & 255u) / 255.0;
  return vec4(srgb_byte_to_linear(r), srgb_byte_to_linear(g),
              srgb_byte_to_linear(b), a);
}

void main() {
  gl_Position = vec4(in_position.x * screen.scale.x - 1.0,
                     1.0 - in_position.y * screen.scale.y, 0.0, 1.0);
  out_uv = in_uv;
  out_color = unpack_rgba8888(in_color);
  out_flags = in_flags;
  out_corner_radius = in_corner_radius;
  out_border_width = in_border_width;
  out_rect_wh = in_rect_wh;
}
)glsl";

  /// GLSL for the GUI's fragment stage. Mirrors `gui_fs_main`.
  constexpr const char GUI_FRAG_GLSL[] = R"glsl(
#version 450

layout(set = 0, binding = 5) uniform texture2D gui_texture;
layout(set = 0, binding = 6) uniform sampler gui_sampler;

layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec4 in_color;
layout(location = 2) flat in uint in_flags;
layout(location = 3) flat in float in_corner_radius;
layout(location = 4) flat in float in_border_width;
layout(location = 5) flat in vec2 in_rect_wh;

layout(location = 0) out vec4 out_color;

float gui_rounded_shape_cover_from_p(vec2 p, vec2 rect_wh, float corner_r) {
  float rw = rect_wh.x;
  float rh = rect_wh.y;
  if (rw <= 0.0 || rh <= 0.0) {
    return 0.0;
  }
  float r = min(max(corner_r, 0.0), min(rw, rh) * 0.5);
  vec2 half_ext = vec2(rw, rh) * 0.5;
  vec2 b = max(half_ext - vec2(r), vec2(0.0));
  vec2 q = abs(p) - b;
  float d = length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;
  float w = max(fwidth(d), 1e-4);
  return 1.0 - smoothstep(-w, w, d);
}

void main() {
  if ((in_flags & 2u) != 0u) {
    out_color = texture(sampler2D(gui_texture, gui_sampler), in_uv) * in_color;
    return;
  }
  vec2 p = vec2((in_uv.x - 0.5) * in_rect_wh.x,
                (in_uv.y - 0.5) * in_rect_wh.y);
  vec4 c = in_color;
  float bw = in_border_width;
  uint rounded_flag = in_flags & 4u;
  if (bw > 1e-5) {
    float cr_o = (rounded_flag != 0u) ? in_corner_radius : 0.0;
    float outer_c = gui_rounded_shape_cover_from_p(p, in_rect_wh, cr_o);
    float irw = max(in_rect_wh.x - 2.0 * bw, 0.0);
    float irh = max(in_rect_wh.y - 2.0 * bw, 0.0);
    float in_r = (rounded_flag != 0u) ? max(in_corner_radius - bw, 0.0) : 0.0;
    float inner_c = gui_rounded_shape_cover_from_p(p, vec2(irw, irh), in_r);
    c.a *= outer_c * (1.0 - inner_c);
    out_color = c;
    return;
  }
  if (rounded_flag != 0u) {
    c.a *= gui_rounded_shape_cover_from_p(p, in_rect_wh, in_corner_radius);
  }
  out_color = c;
}
)glsl";

  /// GLSL for static meshes' vertex stage. Mirrors `mesh_vs_main`.
  constexpr const char MESH_VERT_GLSL[] = R"glsl(
#version 450

layout(set = 0, binding = 1, std140) uniform MeshUniforms {
  mat4 view_projection;
  mat4 model;
} u;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

layout(location = 0) out vec3 out_world_position;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec2 out_uv;

void main() {
  vec4 world = u.model * vec4(in_position, 1.0);
  gl_Position = u.view_projection * world;
  out_world_position = world.xyz;
  // The placement transform is a rotation and a uniform scale, so the same
  // matrix carries the normal; the fragment stage normalizes it again.
  out_normal = (u.model * vec4(in_normal, 0.0)).xyz;
  out_uv = in_uv;
}
)glsl";

  /// GLSL for skinned meshes' vertex stage. Mirrors `skinned_vs_main`;
  /// SKIN_MAX_JOINTS is `MESH_MAX_SKIN_JOINTS` in `skin-palette.h`, and
  /// the palette is `SkinPalette`, three rows of each joint's matrix.
  constexpr const char SKINNED_VERT_GLSL[] = R"glsl(
#version 450

const uint SKIN_MAX_JOINTS = 80u;

layout(set = 0, binding = 1, std140) uniform MeshUniforms {
  mat4 view_projection;
  mat4 model;
} u;

layout(set = 0, binding = 2, std140) uniform SkinPalette {
  vec4 rows[SKIN_MAX_JOINTS * 3u];
} palette;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in uvec4 in_joints;
layout(location = 4) in vec4 in_weights;

layout(location = 0) out vec3 out_world_position;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec2 out_uv;

void main() {
  // Linear blend skinning: the weighted sum of the joints' matrices, built
  // a row at a time, which is what `poseSkinnedMesh` does on the CPU.
  vec4 r0 = vec4(0.0);
  vec4 r1 = vec4(0.0);
  vec4 r2 = vec4(0.0);
  for (uint i = 0u; i < 4u; ++i) {
    uint j = min(in_joints[i], SKIN_MAX_JOINTS - 1u) * 3u;
    r0 += palette.rows[j] * in_weights[i];
    r1 += palette.rows[j + 1u] * in_weights[i];
    r2 += palette.rows[j + 2u] * in_weights[i];
  }
  vec4 p = vec4(in_position, 1.0);
  vec4 n = vec4(in_normal, 0.0);
  vec3 skinned_position = vec3(dot(r0, p), dot(r1, p), dot(r2, p));
  vec3 skinned_normal = vec3(dot(r0, n), dot(r1, n), dot(r2, n));
  vec4 world = u.model * vec4(skinned_position, 1.0);
  gl_Position = u.view_projection * world;
  out_world_position = world.xyz;
  out_normal = (u.model * vec4(skinned_normal, 0.0)).xyz;
  out_uv = in_uv;
}
)glsl";

  /// GLSL for the fragment stage static and skinned meshes share. Mirrors
  /// `mesh_fs_main`; the four constants are `mesh-light.h`'s, restated, and
  /// MESH_MAX_LIGHTS sizes the array, so it and the C++ one move together.
  /// The fifth is `mesh-alpha-cutoff.h`'s.
  constexpr const char MESH_FRAG_GLSL[] = R"glsl(
#version 450

const uint MESH_MAX_LIGHTS = 8u;
const float MESH_LIGHT_AMBIENT = 0.38;
const float MESH_LIGHT_DIFFUSE = 0.62;
const float MESH_LIGHT_POINT = 1.0;
const float MESH_ALPHA_CUTOFF = 0.5;

struct MeshLight {
  vec4 position_range;
  vec4 direction_intensity;
  vec4 color_kind;
};

layout(set = 0, binding = 3, std140) uniform MeshLights {
  uint count;
  uint shade_bands;
  uint pad1;
  uint pad2;
  MeshLight lights[MESH_MAX_LIGHTS];
} lights;

layout(set = 0, binding = 5) uniform texture2D mesh_texture;
// Binding 7 repeats, so a tiling map tiles; 6 is the GUI's clamped one.
layout(set = 0, binding = 7) uniform sampler mesh_sampler;

layout(location = 0) in vec3 in_world_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

layout(location = 0) out vec4 out_color;

// Linear value for an sRGB colour component, for output to an sRGB target.
float mesh_srgb_to_linear(float srgb) {
  if (srgb <= 0.04045) {
    return srgb / 12.92;
  }
  return pow((srgb + 0.055) / 1.055, 2.4);
}

// How much of a point light reaches a surface this far from it: full at the
// light, nothing at its range, and squared in between.
float mesh_falloff(float dist, float range) {
  if (range <= 0.0) {
    return 0.0;
  }
  float reach = clamp(1.0 - dist / range, 0.0, 1.0);
  return reach * reach;
}

// One light's strength flattened into `bands` tones, or left alone for
// fewer than two. `meshShadeBand` in `mesh-style.h`, restated.
float mesh_band(float light, uint bands) {
  if (bands < 2u) {
    return light;
  }
  float top = float(bands - 1u);
  return min(floor(light * float(bands)), top) / top;
}

// What one light adds to a surface.
vec3 mesh_light_contribution(MeshLight light, vec3 world_position,
                             vec3 normal, uint bands) {
  vec3 to_light = light.direction_intensity.xyz;
  float attenuation = 1.0;
  if (light.color_kind.w == MESH_LIGHT_POINT) {
    vec3 offset = light.position_range.xyz - world_position;
    attenuation = mesh_falloff(length(offset), light.position_range.w);
    to_light = offset;
  }
  // A light aimed nowhere lights nothing, rather than dividing by zero.
  float aim = length(to_light);
  if (aim < 1e-4 || attenuation <= 0.0) {
    return vec3(0.0);
  }
  float lambert = clamp(dot(normal, to_light / aim), 0.0, 1.0);
  return light.color_kind.xyz * light.direction_intensity.w *
         mesh_band(lambert * attenuation, bands) * MESH_LIGHT_DIFFUSE;
}

void main() {
  vec3 n = normalize(in_normal);
  vec3 lit = vec3(MESH_LIGHT_AMBIENT);
  for (uint i = 0u; i < lights.count && i < MESH_MAX_LIGHTS; ++i) {
    lit += mesh_light_contribution(lights.lights[i], in_world_position, n,
                                   lights.shade_bands);
  }
  // The map is unorm, so this is the sRGB value the artist authored, shaded
  // and then converted on the way out. An instance with no map samples one
  // texel of a flat colour, so there is no untextured branch.
  vec4 map = texture(sampler2D(mesh_texture, mesh_sampler), in_uv);
  // Alpha-test cutout (ADR-003), which is what lets a sprite billboard go
  // through this pass: its empty corners have to not draw, and the pass is
  // opaque and depth-writing, so the only way for them not to is for their
  // fragments not to exist. Opaque geometry never reaches the branch.
  if (map.a < MESH_ALPHA_CUTOFF) {
    discard;
  }
  vec3 base = clamp(map.rgb * lit, 0.0, 1.0);
  out_color = vec4(mesh_srgb_to_linear(base.r), mesh_srgb_to_linear(base.g),
                   mesh_srgb_to_linear(base.b), 1.0);
}
)glsl";

  /// GLSL for the outline's vertex stage: one triangle covering all of clip
  /// space, from the vertex index alone. Mirrors `outline_vs_main`.
  constexpr const char OUTLINE_VERT_GLSL[] = R"glsl(
#version 450

void main() {
  // (-1,-1), (3,-1) and (-1,3): no seam down a diagonal, no vertex buffer.
  uint id = uint(gl_VertexIndex);
  vec2 corner = vec2(float((id << 1u) & 2u), float(id & 2u));
  gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
)glsl";

  /// GLSL for the outline's fragment stage, which reads the scene's depth
  /// and draws a line wherever it bends. Mirrors `outline_fs_main`;
  /// `OutlineUniforms` is the C++ struct in `mesh-outline-renderer.cpp`.
  constexpr const char OUTLINE_FRAG_GLSL[] = R"glsl(
#version 450

layout(set = 0, binding = 3, std140) uniform OutlineUniforms {
  vec4 color;
  vec4 bounds;
  float width;
  float threshold;
  float pad0;
  float pad1;
} u;

layout(set = 0, binding = 5) uniform texture2D outline_depth;
layout(set = 0, binding = 6) uniform sampler outline_sampler;

layout(location = 0) out vec4 out_color;

// Depth at a pixel, held inside the scissor so that nothing outside what
// the scene drew into is read, and a mesh cut off by it is not lined.
float outline_depth_at(ivec2 p) {
  ivec2 lo = ivec2(u.bounds.xy);
  ivec2 hi = ivec2(u.bounds.zw) - 1;
  return texelFetch(sampler2D(outline_depth, outline_sampler),
                    clamp(p, lo, hi), 0).r;
}

void main() {
  ivec2 p = ivec2(gl_FragCoord.xy);
  float centre = outline_depth_at(p);
  // Nothing was drawn here, so there is nothing to outline.
  if (centre >= 1.0) {
    discard;
  }
  int w = int(u.width);
  float across = outline_depth_at(p + ivec2(w, 0)) +
                 outline_depth_at(p - ivec2(w, 0)) - 2.0 * centre;
  float down = outline_depth_at(p + ivec2(0, w)) +
               outline_depth_at(p - ivec2(0, w)) - 2.0 * centre;
  // Positive where this pixel is nearer than its neighbours on average,
  // which is the near side of an edge: the rim of the thing in front.
  float bend = max(across, down);
  if (bend <= u.threshold) {
    discard;
  }
  float cover = clamp((bend - u.threshold) / max(u.threshold, 1e-9), 0.0, 1.0);
  out_color = vec4(u.color.rgb, u.color.a * cover);
}
)glsl";

  /// GLSL for effects particles' vertex stage: each vertex arrives already
  /// in clip space. Mirrors `fx_vs_main`; the input is `FxVertex`.
  constexpr const char FX_VERT_GLSL[] = R"glsl(
#version 450

layout(location = 0) in vec4 in_clip;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec2 in_shape;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out vec2 out_shape;

void main() {
  gl_Position = in_clip;
  out_color = in_color;
  out_uv = in_uv;
  out_shape = in_shape;
}
)glsl";

  /// GLSL for effects particles' fragment stage, which reads the scene's
  /// depth under each fragment to hide it behind geometry and fade it just
  /// in front. Mirrors `fx_fs_main`; `FxUniforms` is the C++ struct in
  /// `fx-renderer.cpp`.
  constexpr const char FX_FRAG_GLSL[] = R"glsl(
#version 450

layout(set = 0, binding = 3, std140) uniform FxUniforms {
  float softness;
  float pad0;
  float pad1;
  float pad2;
} u;

layout(set = 0, binding = 5) uniform texture2D fx_depth;
layout(set = 0, binding = 6) uniform sampler fx_sampler;

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec2 in_shape;

layout(location = 0) out vec4 out_color;

float fx_hash(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float fx_noise(vec2 p) {
  vec2 cell = floor(p);
  vec2 f = fract(p);
  vec2 s = f * f * (3.0 - 2.0 * f);
  float a = fx_hash(cell);
  float b = fx_hash(cell + vec2(1.0, 0.0));
  float c = fx_hash(cell + vec2(0.0, 1.0));
  float d = fx_hash(cell + vec2(1.0, 1.0));
  return mix(mix(a, b, s.x), mix(c, d, s.x), s.y);
}

float fx_fbm(vec2 p) {
  return fx_noise(p) * 0.65 + fx_noise(p * 2.7 + 5.2) * 0.35;
}

// A disc broken up by noise: soft at the rim, uneven inside, and unlike the
// next particle's, because its seed moves the noise field under it.
float fx_puff(vec2 uv, float seed) {
  float edge = clamp(1.0 - length(uv), 0.0, 1.0);
  vec2 at = uv * 2.3 + vec2(seed * 0.37, seed * 0.71);
  return clamp(edge * edge * (0.35 + 1.15 * fx_fbm(at)), 0.0, 1.0);
}

void main() {
  float scene = texelFetch(sampler2D(fx_depth, fx_sampler),
                           ivec2(gl_FragCoord.xy), 0).r;
  float soft = clamp((scene - gl_FragCoord.z) * u.softness, 0.0, 1.0);
  float disc = clamp(1.0 - dot(in_uv, in_uv), 0.0, 1.0);
  float shape = mix(disc * disc, fx_puff(in_uv, in_shape.y), in_shape.x);
  float cover = shape * soft;
  if (cover <= 0.0) {
    discard;
  }
  out_color = in_color * cover;
}
)glsl";

  /// GLSL for volumetric smoke's vertex stage: each corner arrives in clip
  /// space carrying the ray its fragments march. Mirrors
  /// `fx_volume_vs_main`; the input is `FxVolumeVertex`.
  constexpr const char FX_VOLUME_VERT_GLSL[] = R"glsl(
#version 450

layout(location = 0) in vec4 in_clip;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec4 in_origin;
layout(location = 3) in vec4 in_ray;
layout(location = 4) in vec4 in_params;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_origin;
layout(location = 2) out vec4 out_ray;
layout(location = 3) out vec4 out_params;

void main() {
  gl_Position = in_clip;
  out_color = in_color;
  out_origin = in_origin;
  out_ray = in_ray;
  out_params = in_params;
}
)glsl";

  /// GLSL for volumetric smoke's fragment stage, which marches a ray
  /// through a box of noise until the scene's depth stops it. Mirrors
  /// `fx_volume_fs_main`.
  constexpr const char FX_VOLUME_FRAG_GLSL[] = R"glsl(
#version 450

layout(set = 0, binding = 5) uniform texture2D fxv_depth;
layout(set = 0, binding = 6) uniform sampler fxv_sampler;

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec4 in_origin;
layout(location = 2) in vec4 in_ray;
layout(location = 3) in vec4 in_params;

layout(location = 0) out vec4 out_color;

const int FXV_STEPS = 16;

float fxv_hash(vec3 p) {
  return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

float fxv_noise(vec3 p) {
  vec3 cell = floor(p);
  vec3 f = fract(p);
  vec3 s = f * f * (3.0 - 2.0 * f);
  float x00 = mix(fxv_hash(cell), fxv_hash(cell + vec3(1.0, 0.0, 0.0)), s.x);
  float x10 = mix(fxv_hash(cell + vec3(0.0, 1.0, 0.0)),
                  fxv_hash(cell + vec3(1.0, 1.0, 0.0)), s.x);
  float x01 = mix(fxv_hash(cell + vec3(0.0, 0.0, 1.0)),
                  fxv_hash(cell + vec3(1.0, 0.0, 1.0)), s.x);
  float x11 = mix(fxv_hash(cell + vec3(0.0, 1.0, 1.0)),
                  fxv_hash(cell + vec3(1.0, 1.0, 1.0)), s.x);
  return mix(mix(x00, x10, s.y), mix(x01, x11, s.y), s.z);
}

float fxv_fbm(vec3 p) {
  return fxv_noise(p) * 0.6 + fxv_noise(p * 2.3 + 11.0) * 0.4;
}

// How thick the smoke is at one point of the cloud's own space: an
// ellipsoid gone to nothing at the box's wall, eaten into by noise that the
// cloud's seed moves, so no two clouds are the same shape.
float fxv_density(vec3 p, float seed) {
  float edge = clamp(1.0 - dot(p, p), 0.0, 1.0);
  float n = fxv_fbm(p * 1.9 + seed);
  return edge * edge * clamp(n * 1.7 - 0.45, 0.0, 1.0);
}

void main() {
  vec3 o = in_origin.xyz;
  vec3 d = in_ray.xyz;
  vec3 inv = 1.0 / d;
  vec3 near_wall = (vec3(-1.0) - o) * inv;
  vec3 far_wall = (vec3(1.0) - o) * inv;
  vec3 lo = min(near_wall, far_wall);
  vec3 hi = max(near_wall, far_wall);
  float t_in = max(max(lo.x, lo.y), lo.z);
  // The scene stops the march where a surface is, so the smoke wraps what
  // it meets instead of cutting against it.
  float scene = texelFetch(sampler2D(fxv_depth, fxv_sampler),
                           ivec2(gl_FragCoord.xy), 0).r;
  float t_out = min(min(min(hi.x, hi.y), hi.z),
                    (scene - in_params.x) / in_ray.w);
  if (!(t_out > t_in)) {
    discard;
  }
  float dt = (t_out - t_in) / float(FXV_STEPS);
  float cover = 0.0;
  float through = 1.0;
  for (int i = 0; i < FXV_STEPS; ++i) {
    vec3 p = o + d * (t_in + (float(i) + 0.5) * dt);
    float a = 1.0 - exp(-fxv_density(p, in_origin.w) * in_params.y * dt);
    cover += through * a;
    through *= 1.0 - a;
  }
  if (cover <= 0.0) {
    discard;
  }
  out_color = in_color * cover;
}
)glsl";

  // -------------------------------------------------------------------------
  // Vertex layouts, restated from the C++ vertex structs the way the Metal
  // backend's vertex descriptors restate them.
  // -------------------------------------------------------------------------

  /// GLSL for the water surface's vertex stage. Mirrors `water_vs_main`;
  /// `WaterUniforms` is `water-vertex-uniforms.h`'s `WaterVertexUniforms`.
  constexpr const char WATER_VERT_GLSL[] = R"glsl(
#version 450

layout(set = 0, binding = 1, std140) uniform WaterUniforms {
  mat4 view_projection;
  vec4 field;
} u;

// A surface vertex: where it is, the water's colour in the normal's place,
// and its depth and opacity in the texture coordinate's.
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

layout(location = 0) out vec3 out_world;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out float out_depth;
layout(location = 3) out float out_opacity;
layout(location = 4) out vec3 out_color;

void main() {
  gl_Position = u.view_projection * vec4(in_position, 1.0);
  out_world = in_position;
  out_uv = (in_position.xy - u.field.xy) * u.field.zw;
  out_depth = in_uv.x;
  out_opacity = in_uv.y;
  out_color = in_normal;
}
)glsl";

  /// GLSL for the water surface's fragment stage. Mirrors `water_fs_main`
  /// line for line; `WaterShading` is `water-shading.h`'s.
  constexpr const char WATER_FRAG_GLSL[] = R"glsl(
#version 450

const float WATER_SLOPE_RANGE = 1.0;
const float WATER_LEVEL_RANGE = 0.1;
const float WATER_SHORE_TILES = 2.0;
const float WATER_RIPPLE_GAIN = 3.0;
const float WATER_RIPPLE_SKY = 1.2;
const float WATER_CAUSTIC_LIGHT = 0.25;
const float WATER_BANK_MIN_TILES = 0.3;
const float WATER_BANK_TILES_PER_DEPTH = 0.5;
const float WATER_BANK_MAX_TILES = 2.0;
const uint MESH_MAX_LIGHTS = 8u;
const float MESH_LIGHT_AMBIENT = 0.38;
const float MESH_LIGHT_DIFFUSE = 0.62;
const float MESH_LIGHT_POINT = 1.0;

const vec4 WATER_WAVES[4] = vec4[4](
    vec4(0.80, 0.60, 1.10, 0.10), vec4(-0.45, 0.89, 0.63, 0.08),
    vec4(0.97, -0.24, 0.39, 0.06), vec4(0.20, 0.98, 0.25, 0.05));

layout(set = 0, binding = 3, std140) uniform WaterShading {
  vec4 sky;
  vec4 foam;
  vec4 clarity;
  vec4 light;
  vec4 view;
  vec4 detail;
} s;

struct MeshLight {
  vec4 position_range;
  vec4 direction_intensity;
  vec4 color_kind;
};

// The scene's lights, in the mesh shader's own block.
layout(set = 0, binding = 4, std140) uniform MeshLights {
  uint count;
  uint shade_bands;
  uint pad1;
  uint pad2;
  MeshLight lights[MESH_MAX_LIGHTS];
} lights;

layout(set = 0, binding = 5) uniform texture2D water_field;
// Binding 6 clamps: the field ends at the water's dry ring.
layout(set = 0, binding = 6) uniform sampler water_sampler;

layout(location = 0) in vec3 in_world;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in float in_depth;
layout(location = 3) in float in_opacity;
layout(location = 4) in vec3 in_color;

layout(location = 0) out vec4 out_color;

float water_srgb_to_linear(float srgb) {
  if (srgb <= 0.04045) {
    return srgb / 12.92;
  }
  return pow((srgb + 0.055) / 1.055, 2.4);
}

vec3 water_linear(vec3 c) {
  c = clamp(c, 0.0, 1.0);
  return vec3(water_srgb_to_linear(c.r), water_srgb_to_linear(c.g),
              water_srgb_to_linear(c.b));
}

float water_shelf(float depth, float shore) {
  float run = clamp(WATER_BANK_MIN_TILES + WATER_BANK_TILES_PER_DEPTH * depth,
                    WATER_BANK_MIN_TILES, WATER_BANK_MAX_TILES);
  return smoothstep(0.0, 1.0, clamp(shore / run, 0.0, 1.0));
}

vec2 water_wind_slope(vec2 p, float t, float fine) {
  vec2 slope = vec2(0.0);
  for (int i = 0; i < 4; ++i) {
    vec4 w = WATER_WAVES[i];
    float k = 6.2831853 / w.z;
    float phase = k * (dot(w.xy, p) - 0.55 * sqrt(w.z) * t);
    float weight = i < 2 ? 1.0 : fine;
    slope += w.xy * (w.w * weight * cos(phase));
  }
  return slope;
}

float water_caustic(vec2 p, vec2 slope, float t) {
  vec2 q = p * 3.0 + slope * 1.5;
  float a = sin(q.x + 1.2 * sin(q.y * 1.3 + t * 0.9) + t * 0.6);
  float b = sin(q.y * 1.1 + 1.2 * sin(q.x * 0.9 - t * 0.7) - t * 0.5);
  return pow(clamp(1.0 - abs(a + b), 0.0, 1.0), 4.0);
}

float water_froth(vec2 p, float t) {
  float a = sin(p.x * 9.0 + 1.5 * sin(p.y * 7.0 + t * 1.3) + t * 0.9);
  float b = sin(p.y * 11.0 + 1.5 * sin(p.x * 6.0 - t * 1.1) - t * 0.7);
  return clamp(0.5 + 0.5 * a * b, 0.0, 1.0);
}

float water_falloff(float distance, float range) {
  if (range <= 0.0) {
    return 0.0;
  }
  float reach = clamp(1.0 - distance / range, 0.0, 1.0);
  return reach * reach;
}

float water_band(float light, uint bands) {
  if (bands < 2u) {
    return light;
  }
  float top = float(bands - 1u);
  return min(floor(light * float(bands)), top) / top;
}

// One light's diffuse on the water at p, in rgb, and its glint, in w. The
// light is its three registers: position and range, direction and
// intensity, colour and kind.
vec4 water_light(vec4 position_range, vec4 direction_intensity,
                 vec4 color_kind, vec3 p, vec3 n, vec3 v, uint bands) {
  vec3 to_light = direction_intensity.xyz;
  float attenuation = 1.0;
  if (color_kind.w == MESH_LIGHT_POINT) {
    vec3 offset = position_range.xyz - p;
    attenuation = water_falloff(length(offset), position_range.w);
    to_light = offset;
  }
  float aim = length(to_light);
  if (aim < 1e-4 || attenuation <= 0.0) {
    return vec4(0.0);
  }
  vec3 l = to_light / aim;
  float strength = direction_intensity.w * attenuation;
  float ndh = clamp(dot(n, normalize(l + v)), 0.0, 1.0);
  float glint = pow(ndh, 60.0) + 0.08 * pow(ndh, 12.0);
  return vec4(color_kind.xyz *
                  water_band(clamp(dot(n, l), 0.0, 1.0) * attenuation, bands) *
                  direction_intensity.w * MESH_LIGHT_DIFFUSE,
              strength * glint);
}

void main() {
  vec4 sky = s.sky;
  vec4 foam_color = s.foam;
  vec4 clarity = s.clarity;
  vec4 light = s.light;
  vec4 view = s.view;
  vec4 detail = s.detail;
  vec4 texel = texture(sampler2D(water_field, water_sampler), in_uv);
  float level = (texel.b * 2.0 - 1.0) * WATER_LEVEL_RANGE;
  float depth = in_depth * water_shelf(in_depth, texel.a * WATER_SHORE_TILES);
  float deepness = 1.0 - exp(-depth / max(clarity.z, 1e-3));
  float absorb = clarity.x * pow(clarity.y / clarity.x, clamp(in_opacity, 0.0, 1.0));
  float cover = 1.0 - exp(-absorb * depth);
  vec2 ripple = (texel.rg * 2.0 - 1.0) * (WATER_SLOPE_RANGE * WATER_RIPPLE_GAIN);
  float t = view.w;
  vec2 slope = ripple + water_wind_slope(in_world.xy, t, detail.x) *
                            (detail.z * (0.3 + 0.7 * deepness));
  vec3 n = normalize(vec3(-slope, 1.0));
  vec3 v = view.xyz;
  float fresnel =
      clamp(0.04 + 0.66 * pow(1.0 - clamp(dot(n, v), 0.0, 1.0), 3.0) +
                WATER_RIPPLE_SKY * dot(-ripple, v.xy),
            0.0, 1.0);
  vec3 diffuse = vec3(MESH_LIGHT_AMBIENT);
  vec3 glint = vec3(0.0);
  for (uint i = 0u; i < lights.count && i < MESH_MAX_LIGHTS; ++i) {
    vec4 one = water_light(lights.lights[i].position_range, lights.lights[i].direction_intensity, lights.lights[i].color_kind, in_world, n, v,
                           lights.shade_bands);
    diffuse += one.rgb;
    glint += lights.lights[i].color_kind.xyz * one.w;
  }
  glint *= light.w;
  vec3 water = water_linear(in_color * (1.0 - clarity.w * deepness)) *
               diffuse * (1.0 + 2.0 * level);
  float skylight = min((diffuse.r + diffuse.g + diffuse.b) / 3.0, 1.0);
  vec3 color = water * cover;
  float alpha = cover;
  color = color * (1.0 - fresnel) + water_linear(sky.rgb) * skylight * fresnel;
  alpha = alpha * (1.0 - fresnel) + fresnel;
  color += WATER_CAUSTIC_LIGHT * detail.y * (1.0 - alpha) * diffuse *
           water_caustic(in_world.xy, slope, t);
  float froth = water_froth(in_world.xy, t);
  float edge = 1.0 - smoothstep(0.05, 0.28, texel.a * WATER_SHORE_TILES);
  float crest = smoothstep(0.35, 0.8, level / WATER_LEVEL_RANGE) * foam_color.w;
  float foam = clamp(edge * (0.55 + 0.45 * froth) + crest * froth, 0.0, 1.0);
  color = color * (1.0 - foam) + water_linear(foam_color.rgb) * diffuse * foam;
  alpha = alpha * (1.0 - foam) + foam;
  float shine = max(glint.r, max(glint.g, glint.b));
  out_color = vec4(color + glint, clamp(alpha + shine, 0.0, 1.0));
}
)glsl";

  /// Byte stride of `eng::GuiVertex`, restated from `gui-vertex-layout.h`.
  constexpr uint32_t GUI_VERTEX_STRIDE = 40;

  /// Byte stride of `eng::MeshVertex`: position, normal, uv.
  constexpr uint32_t MESH_VERTEX_STRIDE = 32;

  /// Byte stride of `eng::SkinnedMeshVertex`: a `MeshVertex`, four joint
  /// bytes and four float weights, as `skinned-mesh-vertex.h` asserts.
  constexpr uint32_t SKINNED_VERTEX_STRIDE = 52;

  using Attribute = VkVertexInputAttributeDescription;

  /// `GuiVertex`: position, uv, packed colour, corner radius, border width,
  /// flags, rect size.
  constexpr std::array<Attribute, 7> GUI_ATTRIBUTES{{
      {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
      {1, 0, VK_FORMAT_R32G32_SFLOAT, 8},
      {2, 0, VK_FORMAT_R32_UINT, 16},
      {3, 0, VK_FORMAT_R32_SFLOAT, 20},
      {4, 0, VK_FORMAT_R32_SFLOAT, 24},
      {5, 0, VK_FORMAT_R32_UINT, 28},
      {6, 0, VK_FORMAT_R32G32_SFLOAT, 32},
  }};

  /// `MeshVertex` for the static pipeline, and the first three of the
  /// skinned one.
  constexpr std::array<Attribute, 5> SKINNED_ATTRIBUTES{{
      {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
      {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12},
      {2, 0, VK_FORMAT_R32G32_SFLOAT, 24},
      {3, 0, VK_FORMAT_R8G8B8A8_UINT, 32},
      {4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 36},
  }};

  /// How many of `SKINNED_ATTRIBUTES` a static mesh vertex has.
  constexpr uint32_t MESH_ATTRIBUTE_COUNT = 3;

  /// Byte stride of `eng::FxVertex`: clip position, colour, uv and the
  /// shape pair, as `fx-vertex.h` asserts.
  constexpr uint32_t FX_VERTEX_STRIDE = 48;

  /// `FxVertex`: clip position, premultiplied colour, uv, shape and seed.
  constexpr std::array<Attribute, 4> FX_ATTRIBUTES{{
      {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0},
      {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 16},
      {2, 0, VK_FORMAT_R32G32_SFLOAT, 32},
      {3, 0, VK_FORMAT_R32G32_SFLOAT, 40},
  }};

  /// Byte stride of `eng::FxVolumeVertex`: five float4s, as
  /// `fx-volume-vertex.h` asserts.
  constexpr uint32_t FX_VOLUME_STRIDE = 80;

  /// `FxVolumeVertex`: clip position, colour, ray origin, ray and params.
  constexpr std::array<Attribute, 5> FX_VOLUME_ATTRIBUTES{{
      {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0},
      {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 16},
      {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 32},
      {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 48},
      {4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 64},
  }};

  // -------------------------------------------------------------------------
  // Pipeline assembly
  // -------------------------------------------------------------------------

  /// How a pipeline writes colour: not blended, blended "over" by source
  /// alpha, or blended with colour already multiplied by it.
  enum class BlendMode { OPAQUE, OVER, PREMULTIPLIED };

  /// Whether a pipeline draws into a depth attachment, and whether it
  /// writes it or only tests against it.
  enum class DepthMode { NONE, TEST_AND_WRITE, TEST_ONLY };

  /// Everything that differs between the built-in pipelines.
  struct BuiltinSpec {
    /// Vertex stage source.
    const char* vertex_glsl = nullptr;
    /// Fragment stage source.
    const char* fragment_glsl = nullptr;
    /// Vertex attributes, or null for a pipeline with no vertex buffer.
    const Attribute* attributes = nullptr;
    /// Number of entries in `attributes`.
    uint32_t attribute_count = 0;
    /// Byte stride of the one vertex buffer.
    uint32_t stride = 0;
    /// Colour blending.
    BlendMode blend = BlendMode::OPAQUE;
    /// Depth attachment and test.
    DepthMode depth = DepthMode::NONE;
  };

  /// The device, the layout, and the pass format a pipeline is made for.
  struct BuiltinTarget {
    /// Device the pipeline is created on.
    VkDevice device = VK_NULL_HANDLE;
    /// The shared graphics layout.
    VkPipelineLayout layout = VK_NULL_HANDLE;
    /// Format of the colour attachment it draws into.
    VkFormat color_format = VK_FORMAT_UNDEFINED;
  };

  /// Every create-info a graphics pipeline points into, held together so
  /// the pointers stay valid until `vkCreateGraphicsPipelines` returns.
  struct PipelineParts {
    /// Vertex and fragment stages.
    std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
    /// The one vertex buffer binding.
    VkVertexInputBindingDescription binding{};
    /// Vertex input state.
    VkPipelineVertexInputStateCreateInfo vertex_input{};
    /// Triangle lists, always.
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    /// One viewport and scissor, both dynamic.
    VkPipelineViewportStateCreateInfo viewport{};
    /// Filled, never culled.
    VkPipelineRasterizationStateCreateInfo raster{};
    /// Single-sampled.
    VkPipelineMultisampleStateCreateInfo multisample{};
    /// Depth test and write, when the pipeline has depth.
    VkPipelineDepthStencilStateCreateInfo depth{};
    /// Blend of the one colour attachment.
    VkPipelineColorBlendAttachmentState blend_attachment{};
    /// Blend state over `blend_attachment`.
    VkPipelineColorBlendStateCreateInfo blend{};
    /// Viewport and scissor.
    std::array<VkDynamicState, 2> dynamic_states{};
    /// Dynamic state over `dynamic_states`.
    VkPipelineDynamicStateCreateInfo dynamic{};
    /// Colour attachment format, pointed at by `rendering`.
    VkFormat color_format = VK_FORMAT_UNDEFINED;
    /// Dynamic rendering formats.
    VkPipelineRenderingCreateInfo rendering{};
  };

  /// The two compiled stages of one pipeline.
  struct ShaderPair {
    /// Vertex stage module.
    VkShaderModule vertex = VK_NULL_HANDLE;
    /// Fragment stage module.
    VkShaderModule fragment = VK_NULL_HANDLE;
  };

  VkShaderModule compileModule(VkDevice device, const char* glsl,
                               VkShaderStageFlagBits stage) {
    const std::vector<uint32_t> spirv = compileVulkanGlsl(glsl, stage);
    if (spirv.empty()) {
      return VK_NULL_HANDLE;
    }
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = spirv.size() * sizeof(uint32_t);
    info.pCode = spirv.data();
    VkShaderModule module = VK_NULL_HANDLE;
    vkCreateShaderModule(device, &info, nullptr, &module);
    return module;
  }

  VkPipelineShaderStageCreateInfo stageInfo(VkShaderModule module,
                                            VkShaderStageFlagBits stage) {
    VkPipelineShaderStageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage = stage;
    info.module = module;
    info.pName = "main";
    return info;
  }

  void fillVertexInput(PipelineParts& p, const BuiltinSpec& spec) {
    p.binding = {0, spec.stride, VK_VERTEX_INPUT_RATE_VERTEX};
    p.vertex_input.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    if (spec.attribute_count == 0) {
      return;
    }
    p.vertex_input.vertexBindingDescriptionCount = 1;
    p.vertex_input.pVertexBindingDescriptions = &p.binding;
    p.vertex_input.vertexAttributeDescriptionCount = spec.attribute_count;
    p.vertex_input.pVertexAttributeDescriptions = spec.attributes;
  }

  void fillRasterization(PipelineParts& p) {
    p.input_assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    p.input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    p.viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    p.viewport.viewportCount = 1;
    p.viewport.scissorCount = 1;
    // OBJ files in the wild disagree about winding, and GUI quads are wound
    // whichever way they were built; none of these pipelines culls.
    p.raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    p.raster.polygonMode = VK_POLYGON_MODE_FILL;
    p.raster.cullMode = VK_CULL_MODE_NONE;
    p.raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    p.raster.lineWidth = 1.0f;
    p.multisample.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    p.multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  }

  /// "Over" compositing for the GUI and the outline: RGB by source alpha,
  /// alpha accumulated so the target ends up with the union's coverage.
  void applyOverBlend(VkPipelineColorBlendAttachmentState& a) {
    a.blendEnable = VK_TRUE;
    a.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    a.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    a.colorBlendOp = VK_BLEND_OP_ADD;
    a.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    a.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    a.alphaBlendOp = VK_BLEND_OP_ADD;
  }

  /// Premultiplied compositing for effects: the colour added as it is, and
  /// the target kept by what the particle does not hide.
  void applyPremultipliedBlend(VkPipelineColorBlendAttachmentState& a) {
    applyOverBlend(a);
    a.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  }

  void fillBlend(PipelineParts& p, BlendMode mode) {
    p.blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (mode == BlendMode::OVER) {
      applyOverBlend(p.blend_attachment);
    } else if (mode == BlendMode::PREMULTIPLIED) {
      applyPremultipliedBlend(p.blend_attachment);
    }
    p.blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    p.blend.attachmentCount = 1;
    p.blend.pAttachments = &p.blend_attachment;
  }

  void fillDynamicState(PipelineParts& p) {
    p.dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    p.dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    p.dynamic.dynamicStateCount =
        static_cast<uint32_t>(p.dynamic_states.size());
    p.dynamic.pDynamicStates = p.dynamic_states.data();
  }

  /// Depth state and the attachment formats, which have to agree with the
  /// pass the pipeline is bound in.
  void fillAttachments(PipelineParts& p, const BuiltinSpec& spec,
                       VkFormat color_format) {
    const bool has_depth = spec.depth != DepthMode::NONE;
    p.depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    p.depth.depthTestEnable = has_depth ? VK_TRUE : VK_FALSE;
    p.depth.depthWriteEnable =
        spec.depth == DepthMode::TEST_AND_WRITE ? VK_TRUE : VK_FALSE;
    p.depth.depthCompareOp = VK_COMPARE_OP_LESS;
    p.color_format = color_format;
    p.rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    p.rendering.colorAttachmentCount = 1;
    p.rendering.pColorAttachmentFormats = &p.color_format;
    p.rendering.depthAttachmentFormat =
        has_depth ? VK_FORMAT_D32_SFLOAT : VK_FORMAT_UNDEFINED;
  }

  VkGraphicsPipelineCreateInfo buildCreateInfo(const PipelineParts& p,
                                               VkPipelineLayout layout) {
    VkGraphicsPipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.pNext = &p.rendering;
    info.stageCount = static_cast<uint32_t>(p.stages.size());
    info.pStages = p.stages.data();
    info.pVertexInputState = &p.vertex_input;
    info.pInputAssemblyState = &p.input_assembly;
    info.pViewportState = &p.viewport;
    info.pRasterizationState = &p.raster;
    info.pMultisampleState = &p.multisample;
    info.pDepthStencilState = &p.depth;
    info.pColorBlendState = &p.blend;
    info.pDynamicState = &p.dynamic;
    info.layout = layout;
    return info;
  }

  VkPipeline assemblePipeline(const BuiltinTarget& target,
                              const BuiltinSpec& spec,
                              const ShaderPair& shaders) {
    PipelineParts parts;
    parts.stages[0] = stageInfo(shaders.vertex, VK_SHADER_STAGE_VERTEX_BIT);
    parts.stages[1] = stageInfo(shaders.fragment, VK_SHADER_STAGE_FRAGMENT_BIT);
    fillVertexInput(parts, spec);
    fillRasterization(parts);
    fillBlend(parts, spec.blend);
    fillDynamicState(parts);
    fillAttachments(parts, spec, target.color_format);
    const auto info = buildCreateInfo(parts, target.layout);
    VkPipeline pipeline = VK_NULL_HANDLE;
    vkCreateGraphicsPipelines(target.device, VK_NULL_HANDLE, 1, &info, nullptr,
                              &pipeline);
    return pipeline;
  }

  /// Compile both stages, build the pipeline, and drop the modules, which
  /// the pipeline no longer needs once it exists.
  VkPipeline createBuiltin(const BuiltinTarget& target,
                           const BuiltinSpec& spec) {
    const ShaderPair shaders{compileModule(target.device, spec.vertex_glsl,
                                           VK_SHADER_STAGE_VERTEX_BIT),
                             compileModule(target.device, spec.fragment_glsl,
                                           VK_SHADER_STAGE_FRAGMENT_BIT)};
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (shaders.vertex != VK_NULL_HANDLE &&
        shaders.fragment != VK_NULL_HANDLE) {
      pipeline = assemblePipeline(target, spec, shaders);
    }
    vkDestroyShaderModule(target.device, shaders.vertex, nullptr);
    vkDestroyShaderModule(target.device, shaders.fragment, nullptr);
    return pipeline;
  }

}  // namespace

VkPipeline createVulkanGuiPipeline(VkDevice device, VkPipelineLayout layout,
                                   VkFormat color_format) {
  const BuiltinSpec spec{
      GUI_VERT_GLSL,         GUI_FRAG_GLSL,
      GUI_ATTRIBUTES.data(), static_cast<uint32_t>(GUI_ATTRIBUTES.size()),
      GUI_VERTEX_STRIDE,     BlendMode::OVER,
      DepthMode::NONE};
  return createBuiltin({device, layout, color_format}, spec);
}

VkPipeline createVulkanMeshPipeline(VkDevice device, VkPipelineLayout layout,
                                    VkFormat color_format) {
  const BuiltinSpec spec{
      MESH_VERT_GLSL,           MESH_FRAG_GLSL,     SKINNED_ATTRIBUTES.data(),
      MESH_ATTRIBUTE_COUNT,     MESH_VERTEX_STRIDE, BlendMode::OPAQUE,
      DepthMode::TEST_AND_WRITE};
  return createBuiltin({device, layout, color_format}, spec);
}

VkPipeline createVulkanSkinnedMeshPipeline(VkDevice device,
                                           VkPipelineLayout layout,
                                           VkFormat color_format) {
  // Same fragment stage, blend and depth as static meshes: the two draw
  // into one pass and resolve against each other in its depth buffer.
  const BuiltinSpec spec{SKINNED_VERT_GLSL,
                         MESH_FRAG_GLSL,
                         SKINNED_ATTRIBUTES.data(),
                         static_cast<uint32_t>(SKINNED_ATTRIBUTES.size()),
                         SKINNED_VERTEX_STRIDE,
                         BlendMode::OPAQUE,
                         DepthMode::TEST_AND_WRITE};
  return createBuiltin({device, layout, color_format}, spec);
}

VkPipeline createVulkanOutlinePipeline(VkDevice device, VkPipelineLayout layout,
                                       VkFormat color_format) {
  // It reads the depth texture rather than testing against it: the pass it
  // draws in has no depth attachment, since that one is being sampled.
  const BuiltinSpec spec{OUTLINE_VERT_GLSL, OUTLINE_FRAG_GLSL, nullptr, 0, 0,
                         BlendMode::OVER,   DepthMode::NONE};
  return createBuiltin({device, layout, color_format}, spec);
}

VkPipeline createVulkanFxPipeline(VkDevice device, VkPipelineLayout layout,
                                  VkFormat color_format) {
  // Like the outline, it reads the scene's depth as a texture, so the pass
  // it draws in has no depth attachment.
  const BuiltinSpec spec{
      FX_VERT_GLSL,         FX_FRAG_GLSL,
      FX_ATTRIBUTES.data(), static_cast<uint32_t>(FX_ATTRIBUTES.size()),
      FX_VERTEX_STRIDE,     BlendMode::PREMULTIPLIED,
      DepthMode::NONE};
  return createBuiltin({device, layout, color_format}, spec);
}

VkPipeline createVulkanFxVolumePipeline(VkDevice device,
                                        VkPipelineLayout layout,
                                        VkFormat color_format) {
  // The same pass and the same blending as the particles; what differs is
  // the ray each fragment marches rather than the quad it fades.
  const BuiltinSpec spec{FX_VOLUME_VERT_GLSL,
                         FX_VOLUME_FRAG_GLSL,
                         FX_VOLUME_ATTRIBUTES.data(),
                         static_cast<uint32_t>(FX_VOLUME_ATTRIBUTES.size()),
                         FX_VOLUME_STRIDE,
                         BlendMode::PREMULTIPLIED,
                         DepthMode::NONE};
  return createBuiltin({device, layout, color_format}, spec);
}

VkPipeline createVulkanWaterPipeline(VkDevice device, VkPipelineLayout layout,
                                     VkFormat color_format) {
  // In the scene pass, after the opaque meshes: tested against their depth
  // so a crate in a pond hides it, and not written, so the outline and the
  // effects still see the ground under it. Premultiplied, since it both
  // hides some of that ground and adds light to the rest.
  const BuiltinSpec spec{WATER_VERT_GLSL,           WATER_FRAG_GLSL,
                         SKINNED_ATTRIBUTES.data(), MESH_ATTRIBUTE_COUNT,
                         MESH_VERTEX_STRIDE,        BlendMode::PREMULTIPLIED,
                         DepthMode::TEST_ONLY};
  return createBuiltin({device, layout, color_format}, spec);
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
