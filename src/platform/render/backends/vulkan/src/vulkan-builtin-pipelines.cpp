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

// The shared body below is written once, in types every backend reads,
// and spliced into each: these say what its words mean in GLSL.
#define float2 vec2
#define float3 vec3
#define float4 vec4
#define int2 ivec2
#define float4x4 mat4
#define saturate(x) clamp((x), 0.0f, 1.0f)
#define WATER_CONST const
#define WATER_P
#define WATER_PC
#define WATER_A
#define WATER_AC
#define water_mul(m, v) ((m) * (v))

layout(set = 0, binding = 3, std140) uniform WaterShading {
  mat4 view_projection;
  vec4 sky;
  vec4 foam;
  vec4 clarity;
  vec4 absorb;
  vec4 light;
  vec4 view;
  vec4 detail;
  vec4 screen;
  vec4 surface;
  vec4 texel;
  vec4 toggles;
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
  MeshLight lights[8];
} lights;

layout(set = 0, binding = 5) uniform texture2D water_field;
layout(set = 0, binding = 8) uniform texture2D water_scene;
layout(set = 0, binding = 9) uniform texture2D water_depth;
layout(set = 0, binding = 10) uniform texture2D water_still;
// Binding 6 clamps: the field ends at the water's dry ring.
layout(set = 0, binding = 6) uniform sampler water_sampler;

layout(location = 0) in vec3 in_world;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in float in_depth;
layout(location = 3) in float in_opacity;
layout(location = 4) in vec3 in_color;

layout(location = 0) out vec4 out_color;

float4 water_field_at(float2 uv) {
  return texture(sampler2D(water_field, water_sampler), uv);
}

float4 water_still_at(float2 uv) {
  return texture(sampler2D(water_still, water_sampler), uv);
}

float3 water_scene_at(float2 uv) {
  return texture(sampler2D(water_scene, water_sampler), uv).rgb;
}

float water_scene_depth(int2 p) {
  int2 top = textureSize(sampler2D(water_depth, water_sampler), 0) - 1;
  return texelFetch(sampler2D(water_depth, water_sampler),
                    clamp(p, int2(0), top), 0).r;
}

uint water_light_count() { return lights.count; }

uint water_shade_bands() { return lights.shade_bands; }

float4 water_light_register(uint i, int k) {
  return k == 0 ? lights.lights[i].position_range
                : k == 1 ? lights.lights[i].direction_intensity
                         : lights.lights[i].color_kind;
}

// Where a point in clip space lands on the copy of the scene: rows run
// down from the top, as the viewport's flip leaves them.
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
  float wet = 1.0f - smoothstep(0.0f, 1.0f, still.a);
  if (wet <= 0.0f || still.r * WATER_SHORE_TILES > 0.25f) {
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
  float land = still.a * WATER_WET_TILES;
  float run = water_lap_run(world.xy, s.view.w) * s.light.y;
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
  float shore = still.r * WATER_SHORE_TILES;
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
  float near_bank = (1.0f - smoothstep(0.0f, WATER_LAP_REACH, shore)) * s.light.y;
  ripple += water_shore_way(WATER_AC uv) *
            (WATER_LAP_HEIGHT * 6.2831853f / WATER_LAP_WAVELENGTH *
             cos(lap) * near_bank *
             water_resolved(WATER_LAP_WAVELENGTH, pixel));
  // The shallows are sheltered: the wind raises less there, and a still
  // surface raises none.
  float calm = s.detail.z * (0.3f + 0.7f * deepness);
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
  ground *= 1.0f + WATER_CAUSTIC_LIGHT * s.detail.y *
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

void main() {
  out_color = water_shade(in_world, in_uv, in_depth, in_opacity, in_color,
                          gl_FragCoord);
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

  /// Whether a pipeline draws into a depth attachment, testing and
  /// writing it.
  enum class DepthMode { NONE, TEST_AND_WRITE };

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
  // In the pass over the scene, as the effects are: no depth attachment,
  // since the scene's depth is a texture the water reads to hide itself
  // behind what stands in front of it. Premultiplied, as they are.
  const BuiltinSpec spec{
      WATER_VERT_GLSL,      WATER_FRAG_GLSL,    SKINNED_ATTRIBUTES.data(),
      MESH_ATTRIBUTE_COUNT, MESH_VERTEX_STRIDE, BlendMode::PREMULTIPLIED,
      DepthMode::NONE};
  return createBuiltin({device, layout, color_format}, spec);
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
