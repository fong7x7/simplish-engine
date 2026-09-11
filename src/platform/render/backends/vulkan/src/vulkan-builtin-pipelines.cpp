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
  constexpr const char MESH_FRAG_GLSL[] = R"glsl(
#version 450

const uint MESH_MAX_LIGHTS = 8u;
const float MESH_LIGHT_AMBIENT = 0.38;
const float MESH_LIGHT_DIFFUSE = 0.62;
const float MESH_LIGHT_POINT = 1.0;

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
  vec3 texel = texture(sampler2D(mesh_texture, mesh_sampler), in_uv).rgb;
  vec3 base = clamp(texel * lit, 0.0, 1.0);
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

  // -------------------------------------------------------------------------
  // Vertex layouts, restated from the C++ vertex structs the way the Metal
  // backend's vertex descriptors restate them.
  // -------------------------------------------------------------------------

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

  // -------------------------------------------------------------------------
  // Pipeline assembly
  // -------------------------------------------------------------------------

  /// How a pipeline writes colour.
  enum class BlendMode { OPAQUE, OVER };

  /// Whether a pipeline draws into a depth attachment.
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

  void fillBlend(PipelineParts& p, BlendMode mode) {
    p.blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (mode == BlendMode::OVER) {
      applyOverBlend(p.blend_attachment);
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
    const bool has_depth = spec.depth == DepthMode::TEST_AND_WRITE;
    p.depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    p.depth.depthTestEnable = has_depth ? VK_TRUE : VK_FALSE;
    p.depth.depthWriteEnable = has_depth ? VK_TRUE : VK_FALSE;
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

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
