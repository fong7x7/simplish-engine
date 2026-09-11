#include "vulkan-glsl-compiler.h"

#ifdef ENGINE_RENDERER_VULKAN

#include <engine/core/logger.h>
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>
#include <string>

namespace eng::render {

namespace {

  /// GLSL version assumed when a source has no `#version` line.
  constexpr int GLSL_DEFAULT_VERSION = 450;

  /// Vulkan semantics (sets, bindings, `gl_VertexIndex`) and SPIR-V output.
  constexpr auto GLSL_MESSAGES = static_cast<glslang_messages_t>(
      GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT);

  glslang_stage_t toGlslangStage(VkShaderStageFlagBits stage) {
    return stage == VK_SHADER_STAGE_FRAGMENT_BIT ? GLSLANG_STAGE_FRAGMENT
                                                 : GLSLANG_STAGE_VERTEX;
  }

  /// SPIR-V 1.3 is what Vulkan 1.1 promises; nothing here needs later.
  glslang_input_t buildInput(const char* source, glslang_stage_t stage) {
    glslang_input_t in{};
    in.language = GLSLANG_SOURCE_GLSL;
    in.stage = stage;
    in.client = GLSLANG_CLIENT_VULKAN;
    in.client_version = GLSLANG_TARGET_VULKAN_1_3;
    in.target_language = GLSLANG_TARGET_SPV;
    in.target_language_version = GLSLANG_TARGET_SPV_1_3;
    in.code = source;
    in.default_version = GLSL_DEFAULT_VERSION;
    in.default_profile = GLSLANG_NO_PROFILE;
    in.messages = GLSL_MESSAGES;
    in.resource = glslang_default_resource();
    return in;
  }

  void logFailure(const char* step, const char* log) {
    LOG_ERROR("render", std::string("Vulkan built-in GLSL failed to ") + step +
                            ":\n" + (log != nullptr ? log : ""));
  }

  bool parseShader(glslang_shader_t* shader, const glslang_input_t& input) {
    if (glslang_shader_preprocess(shader, &input) == 0 ||
        glslang_shader_parse(shader, &input) == 0) {
      logFailure("compile", glslang_shader_get_info_log(shader));
      return false;
    }
    return true;
  }

  std::vector<uint32_t> linkAndGenerate(glslang_shader_t* shader,
                                        glslang_stage_t stage) {
    glslang_program_t* program = glslang_program_create();
    glslang_program_add_shader(program, shader);
    std::vector<uint32_t> words;
    if (glslang_program_link(program, GLSL_MESSAGES) != 0) {
      glslang_program_SPIRV_generate(program, stage);
      words.resize(glslang_program_SPIRV_get_size(program));
      glslang_program_SPIRV_get(program, words.data());
    } else {
      logFailure("link", glslang_program_get_info_log(program));
    }
    glslang_program_delete(program);
    return words;
  }

}  // namespace

std::vector<uint32_t> compileVulkanGlsl(const char* source,
                                        VkShaderStageFlagBits stage) {
  // Reference-counted inside glslang, so pairing it per call is safe.
  glslang_initialize_process();
  const glslang_stage_t gl_stage = toGlslangStage(stage);
  const glslang_input_t input = buildInput(source, gl_stage);
  glslang_shader_t* shader = glslang_shader_create(&input);
  std::vector<uint32_t> words;
  if (parseShader(shader, input)) {
    words = linkAndGenerate(shader, gl_stage);
  }
  glslang_shader_delete(shader);
  glslang_finalize_process();
  return words;
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
