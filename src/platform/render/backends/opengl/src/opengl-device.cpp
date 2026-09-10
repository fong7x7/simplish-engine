#include <engine/render/backends/opengl/opengl-command-list.h>
#include <engine/render/backends/opengl/opengl-device.h>
#include <engine/render/backends/opengl/opengl-types.h>

#ifdef ENGINE_RENDERER_OPENGL

// SDL headers trigger -Wold-style-cast in compile-time asserts; suppress.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#include <SDL3/SDL.h>
#pragma clang diagnostic pop
#include <cstring>
#include <engine/render/rhi-buffer-desc.h>
#include <engine/render/rhi-capture-request.h>
#include <engine/render/rhi-capture-result.h>
#include <engine/render/rhi-compute-pipeline-desc.h>
#include <engine/render/rhi-graphics-pipeline-desc.h>
#include <engine/render/rhi-shader-desc.h>
#include <engine/render/rhi-texture-desc.h>
#include <engine/render/rhi-texture-update-2d.h>

// stb_image_write — PNG/JPEG encoding for capture API.
// Implementation lives in stb-image-write-impl.cpp.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wsign-conversion"
#include <stb_image_write.h>
#pragma clang diagnostic pop

#include <fstream>

// glad must be included after SDL for correct GL loader setup.
// On systems without glad, provide stub GL functions for compilation.
#if __has_include(<glad/glad.h>)
#include <glad/glad.h>
#else
// Minimal GL stubs for builds without glad (CI, headless).
// tryCreate() will fail at runtime when gladLoadGLLoader returns 0.
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_COPY_READ_BUFFER 0x8F36
#define GL_COPY_WRITE_BUFFER 0x8F37
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_FRAMEBUFFER_SRGB 0x8DB9
#define GL_BLEND 0x0BE2
#define GL_DEPTH_TEST 0x0B71
#define GL_SCISSOR_TEST 0x0C11
#define GL_CULL_FACE 0x0B44
#define GL_FRONT 0x0404
#define GL_BACK 0x0405
#define GL_CW 0x0900
#define GL_CCW 0x0901
#define GL_FILL 0x1B02
#define GL_LINE 0x1B01
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_MAP_READ_BIT 0x0001
#define GL_MAP_WRITE_BIT 0x0002
#define GL_MAP_PERSISTENT_BIT 0x0040
#define GL_MAP_COHERENT_BIT 0x0080
#define GL_ALL_BARRIER_BITS 0xFFFFFFFF
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPUTE_SHADER 0x91B9
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE0 0x84C0
#define GL_UNSIGNED_INT 0x1405
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ONE 1
#define GL_ZERO 0
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_PIXEL_PACK_BUFFER 0x88EB
#define GL_FLOAT 0x1406
#define GL_FALSE 0

using GLboolean = unsigned char;
/// What GL calls a size or a count. Missing from these stubs until the mesh
/// pipeline needed one, which is why a build without glad did not compile.
using GLsizei = int;

// Stub function declarations — these never run because tryCreate fails first.
inline int gladLoadGLLoader(void* (* /*load*/)(const char*)) {
  return 0;
}
inline const unsigned char* glGetString(GLenum /*name*/) {
  return nullptr;
}
inline void glGenBuffers(int /*n*/, GLuint* /*buffers*/) {}
inline void glDeleteBuffers(int /*n*/, const GLuint* /*buffers*/) {}
inline void glBindBuffer(GLenum /*target*/, GLuint /*buffer*/) {}
inline void glBufferStorage(GLenum /*target*/, int64_t /*size*/,
                            const void* /*data*/, uint32_t /*flags*/) {}
inline void* glMapBufferRange(GLenum /*target*/, int64_t /*offset*/,
                              int64_t /*length*/, uint32_t /*access*/) {
  return nullptr;
}
inline void glUnmapBuffer(GLenum /*target*/) {}
inline void glGenTextures(int /*n*/, GLuint* /*textures*/) {}
inline void glDeleteTextures(int /*n*/, const GLuint* /*textures*/) {}
inline void glBindTexture(GLenum /*target*/, GLuint /*texture*/) {}
inline void glActiveTexture(GLenum /*texture*/) {}
inline void glTexStorage2D(GLenum /*target*/, int /*levels*/,
                           int /*internal_format*/, int /*width*/,
                           int /*height*/) {}
inline void glTexSubImage2D(GLenum /*target*/, int /*level*/, int /*xoffset*/,
                            int /*yoffset*/, int /*width*/, int /*height*/,
                            GLenum /*format*/, GLenum /*type*/,
                            const void* /*pixels*/) {}
inline GLuint glCreateShader(GLenum /*type*/) {
  return 0;
}
inline void glShaderSource(GLuint /*shader*/, int /*count*/,
                           const char* const* /*string*/,
                           const int* /*length*/) {}
inline void glCompileShader(GLuint /*shader*/) {}
inline void glGetShaderiv(GLuint /*shader*/, GLenum /*pname*/,
                          int* /*params*/) {}
inline void glDeleteShader(GLuint /*shader*/) {}
inline GLuint glCreateProgram() {
  return 0;
}
inline void glAttachShader(GLuint /*program*/, GLuint /*shader*/) {}
inline void glLinkProgram(GLuint /*program*/) {}
inline void glGetProgramiv(GLuint /*program*/, GLenum /*pname*/,
                           int* /*params*/) {}
inline void glDeleteProgram(GLuint /*program*/) {}
inline void glUseProgram(GLuint /*program*/) {}
inline GLint glGetUniformLocation(GLuint /*program*/, const char* /*name*/) {
  return -1;
}
inline void glUniform1ui(GLint /*location*/, GLuint /*value*/) {}
inline void glUniform4fv(GLint /*location*/, int /*count*/,
                         const float* /*value*/) {}
inline void glUniformMatrix4fv(GLint /*location*/, int /*count*/,
                               GLboolean /*transpose*/,
                               const float* /*value*/) {}
inline void glUniform2fv(GLint /*location*/, int /*count*/,
                         const float* /*value*/) {}
inline void glBlendFuncSeparate(GLenum /*src_rgb*/, GLenum /*dst_rgb*/,
                                GLenum /*src_alpha*/, GLenum /*dst_alpha*/) {}
inline void glGenVertexArrays(int /*n*/, GLuint* /*arrays*/) {}
inline void glDeleteVertexArrays(int /*n*/, const GLuint* /*arrays*/) {}
inline void glBindVertexArray(GLuint /*array*/) {}
inline void glEnableVertexAttribArray(GLuint /*index*/) {}
inline void glVertexAttribFormat(GLuint /*attrib_index*/, int /*size*/,
                                 GLenum /*type*/, unsigned char /*normalized*/,
                                 GLuint /*relative_offset*/) {}
inline void glVertexAttribIFormat(GLuint /*attrib_index*/, int /*size*/,
                                  GLenum /*type*/, GLuint /*relative_offset*/) {
}
inline void glVertexAttribBinding(GLuint /*attrib_index*/,
                                  GLuint /*binding_index*/) {}
inline void glVertexBindingDivisor(GLuint /*binding_index*/,
                                   GLuint /*divisor*/) {}
inline void glBindVertexBuffer(GLuint /*binding_index*/, GLuint /*buffer*/,
                               int64_t /*offset*/, int /*stride*/) {}
inline void glViewport(int /*x*/, int /*y*/, int /*width*/, int /*height*/) {}
inline void glDepthRangef(float /*near_val*/, float /*far_val*/) {}
inline void glScissor(int /*x*/, int /*y*/, int /*width*/, int /*height*/) {}
inline void glEnable(GLenum /*cap*/) {}
inline void glDisable(GLenum /*cap*/) {}
inline void glPixelStorei(GLenum /*pname*/, GLint /*param*/) {}
inline void glCullFace(GLenum /*mode*/) {}
inline void glFrontFace(GLenum /*mode*/) {}
inline void glPolygonMode(GLenum /*face*/, GLenum /*mode*/) {}
inline void glDepthMask(unsigned char /*flag*/) {}
inline void glClearColor(float /*red*/, float /*green*/, float /*blue*/,
                         float /*alpha*/) {}
inline void glClearDepthf(float /*depth*/) {}
inline void glClear(uint32_t /*mask*/) {}
inline void glDrawArraysInstanced(GLenum /*mode*/, int /*first*/, int /*count*/,
                                  int /*instance_count*/) {}
inline void glDrawElementsInstanced(GLenum /*mode*/, int /*count*/,
                                    GLenum /*type*/, const void* /*indices*/,
                                    int /*instance_count*/) {}
inline void glDispatchCompute(GLuint /*num_groups_x*/, GLuint /*num_groups_y*/,
                              GLuint /*num_groups_z*/) {}
inline void glMemoryBarrier(uint32_t /*barriers*/) {}
inline void glCopyBufferSubData(GLenum /*read_target*/, GLenum /*write_target*/,
                                int64_t /*read_offset*/,
                                int64_t /*write_offset*/, int64_t /*size*/) {}
inline void glReadPixels(int /*x*/, int /*y*/, int /*width*/, int /*height*/,
                         GLenum /*format*/, GLenum /*type*/, void* /*pixels*/) {
}
inline void glBindFramebuffer(GLenum /*target*/, GLuint /*framebuffer*/) {}
inline void glFinish() {}
inline GLboolean glIsEnabled(GLenum /*cap*/) {
  return 0;
}
inline void glGenFramebuffers(int /*n*/, GLuint* /*framebuffers*/) {}
inline void glDeleteFramebuffers(int /*n*/, const GLuint* /*framebuffers*/) {}
inline void glFramebufferTexture2D(GLenum /*target*/, GLenum /*attachment*/,
                                   GLenum /*textarget*/, GLuint /*texture*/,
                                   int /*level*/) {}
inline void glCopyTexSubImage2D(GLenum /*target*/, int /*level*/,
                                int /*xoffset*/, int /*yoffset*/, int /*x*/,
                                int /*y*/, GLsizei /*width*/,
                                GLsizei /*height*/) {}
inline const unsigned char* glGetStringi(GLenum /*name*/, GLuint /*index*/) {
  return nullptr;
}
inline void glGetIntegerv(GLenum /*pname*/, int* /*data*/) {}
#endif

#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#endif

namespace eng::render {

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

OpenGlDevice::OpenGlDevice(const RenderConfig& config)
  : window_(static_cast<SDL_Window*>(config.native_window)), config_(config) {}

OpenGlDevice::~OpenGlDevice() {
  if (gl_context_ != nullptr) {
    SDL_GL_DestroyContext(static_cast<SDL_GLContext>(gl_context_));
  }
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

void OpenGlDevice::setGlAttributes() {
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
}

void* OpenGlDevice::createGlContext(SDL_Window* window) {
  setGlAttributes();
  auto* gl_ctx = SDL_GL_CreateContext(window);
  if (gl_ctx == nullptr) {
    return nullptr;
  }
  if (gladLoadGLLoader(reinterpret_cast<void* (*)(const char*)>(
          SDL_GL_GetProcAddress)) == 0) {
    SDL_GL_DestroyContext(gl_ctx);
    return nullptr;
  }
  return gl_ctx;
}

void OpenGlDevice::initFromContext(void* gl_ctx) {
  gl_context_ = gl_ctx;
  populateCaps();
  backbuffer_handle_ = allocHandle();
}

std::optional<std::unique_ptr<OpenGlDevice>>
OpenGlDevice::tryCreate(const RenderConfig& config) {
  if (config.native_window == nullptr) {
    return std::nullopt;
  }
  auto* window = static_cast<SDL_Window*>(config.native_window);
  auto* gl_ctx = createGlContext(window);
  if (gl_ctx == nullptr) {
    return std::nullopt;
  }
  auto device = std::unique_ptr<OpenGlDevice>(new OpenGlDevice(config));
  device->initFromContext(gl_ctx);
  return device;
}

// ---------------------------------------------------------------------------
// Identification
// ---------------------------------------------------------------------------

RhiBackend OpenGlDevice::backend() const {
  return RhiBackend::OPEN_GL;
}

const RhiDeviceCapabilities& OpenGlDevice::capabilities() const {
  return caps_;
}

// ---------------------------------------------------------------------------
// Handle allocation
// ---------------------------------------------------------------------------

RhiBufferHandle OpenGlDevice::allocHandle() {
  return next_handle_++;
}

// ---------------------------------------------------------------------------
// Buffer lifecycle
// ---------------------------------------------------------------------------

GLuint OpenGlDevice::createGlBuffer(const RhiBufferDesc& desc) {
  GLuint gl_id = 0;
  glGenBuffers(1, &gl_id);
  if (gl_id == 0) {
    return 0;
  }
  auto visibility = desc.host_visible ? GlHostVisibility::HOST_VISIBLE
                                      : GlHostVisibility::DEVICE_LOCAL;
  glBindBuffer(GL_ARRAY_BUFFER, gl_id);
  glBufferStorage(GL_ARRAY_BUFFER, static_cast<int64_t>(desc.size), nullptr,
                  toGlStorageFlags(desc.usage, visibility));
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  return gl_id;
}

RhiBufferHandle OpenGlDevice::createBuffer(const RhiBufferDesc& desc) {
  auto gl_id = createGlBuffer(desc);
  if (gl_id == 0) {
    return RHI_BUFFER_INVALID;
  }
  auto handle = allocHandle();
  buffers_[handle] = {gl_id, desc.size, nullptr};
  return handle;
}

void OpenGlDevice::destroyBuffer(RhiBufferHandle handle) {
  if (!buffers_.contains(handle)) {
    return;
  }
  auto& entry = buffers_[handle];
  glDeleteBuffers(1, &entry.gl_id);
  buffers_.erase(handle);
}

void* OpenGlDevice::mapGlBuffer(GlBufferEntry& entry) {
  constexpr uint32_t MAP_FLAGS = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT |
                                 GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
  glBindBuffer(GL_ARRAY_BUFFER, entry.gl_id);
  entry.mapped_ptr = glMapBufferRange(
      GL_ARRAY_BUFFER, 0, static_cast<int64_t>(entry.size), MAP_FLAGS);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  return entry.mapped_ptr;
}

void* OpenGlDevice::mapBuffer(RhiBufferHandle handle) {
  if (!buffers_.contains(handle)) {
    return nullptr;
  }
  auto& entry = buffers_[handle];
  if (entry.mapped_ptr != nullptr) {
    return entry.mapped_ptr;
  }
  return mapGlBuffer(entry);
}

void OpenGlDevice::unmapBuffer(RhiBufferHandle handle) {
  if (!buffers_.contains(handle)) {
    return;
  }
  auto& entry = buffers_[handle];
  if (entry.mapped_ptr == nullptr) {
    return;
  }

  glBindBuffer(GL_ARRAY_BUFFER, entry.gl_id);
  glUnmapBuffer(GL_ARRAY_BUFFER);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  entry.mapped_ptr = nullptr;
}

// ---------------------------------------------------------------------------
// Texture lifecycle
// ---------------------------------------------------------------------------

void OpenGlDevice::uploadTexturePixels(const RhiTextureDesc& desc,
                                       const GlFormatInfo& fmt) {
  if (desc.initial_pixels == nullptr) {
    return;
  }
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, static_cast<int>(desc.width),
                  static_cast<int>(desc.height), fmt.format, fmt.type,
                  desc.initial_pixels);
}

GLuint OpenGlDevice::createGlTexture(const RhiTextureDesc& desc) {
  GLuint gl_id = 0;
  glGenTextures(1, &gl_id);
  if (gl_id == 0) {
    return 0;
  }
  auto fmt = GlFormatInfo::fromRhiFormat(desc.format);
  glBindTexture(GL_TEXTURE_2D, gl_id);
  auto w = static_cast<int>(desc.width);
  auto h = static_cast<int>(desc.height);
  glTexStorage2D(GL_TEXTURE_2D, static_cast<int>(desc.mip_levels),
                 fmt.internal_format, w, h);
  uploadTexturePixels(desc, fmt);
  glBindTexture(GL_TEXTURE_2D, 0);
  return gl_id;
}

RhiTextureHandle OpenGlDevice::createTexture(const RhiTextureDesc& desc) {
  auto gl_id = createGlTexture(desc);
  if (gl_id == 0) {
    return RHI_TEXTURE_INVALID;
  }
  auto handle = allocHandle();
  textures_[handle] = {gl_id, desc.width, desc.height, desc.format, desc.usage};
  return handle;
}

void OpenGlDevice::destroyTexture(RhiTextureHandle handle) {
  if (!textures_.contains(handle)) {
    return;
  }
  auto& entry = textures_[handle];
  glDeleteTextures(1, &entry.gl_id);
  textures_.erase(handle);
}

bool OpenGlDevice::updateTexture2D(RhiTextureHandle h,
                                   const RhiTextureUpdate2D& u) {
  auto it = textures_.find(h);
  if (it == textures_.end() || u.pixels == nullptr || u.width == 0 ||
      u.height == 0 || u.format == RhiFormat::UNDEFINED) {
    return false;
  }
  auto& ent = it->second;
  if (u.format != ent.format) {
    return false;
  }
  if (u.offset_x + u.width > ent.width || u.offset_y + u.height > ent.height) {
    return false;
  }
  const uint32_t bpp = GlFormatInfo::bytesPerTexel(ent.format);
  if (bpp == 0) {
    return false;
  }
  uint32_t row_bytes = u.bytes_per_row;
  if (row_bytes == 0) {
    row_bytes = u.width * bpp;
  }
  if (row_bytes < u.width * bpp || row_bytes % bpp != 0) {
    return false;
  }
  auto fmt = GlFormatInfo::fromRhiFormat(ent.format);
  glBindTexture(GL_TEXTURE_2D, ent.gl_id);
  const GLint row_texels = static_cast<GLint>(row_bytes / bpp);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, row_texels);
  glTexSubImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(u.offset_x),
                  static_cast<GLint>(u.offset_y), static_cast<GLsizei>(u.width),
                  static_cast<GLsizei>(u.height), fmt.format, fmt.type,
                  u.pixels);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
  return true;
}

// ---------------------------------------------------------------------------
// Shader lifecycle
// ---------------------------------------------------------------------------

GLuint OpenGlDevice::checkShaderCompile(GLuint shader) {
  int status = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status == 0) {
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

GLuint OpenGlDevice::compileGlShader(const RhiShaderDesc& desc) {
  GLuint shader = glCreateShader(shaderStageToGl(desc.stage));
  if (shader == 0) {
    return 0;
  }
  const auto* src = reinterpret_cast<const char*>(desc.bytecode);
  auto len = static_cast<int>(desc.bytecode_size);
  glShaderSource(shader, 1, &src, &len);
  glCompileShader(shader);
  return checkShaderCompile(shader);
}

RhiShaderHandle OpenGlDevice::createShader(const RhiShaderDesc& desc) {
  auto shader = compileGlShader(desc);
  if (shader == 0) {
    return RHI_SHADER_INVALID;
  }
  auto handle = allocHandle();
  shaders_[handle] = shader;
  return handle;
}

void OpenGlDevice::destroyShader(RhiShaderHandle handle) {
  if (!shaders_.contains(handle)) {
    return;
  }
  glDeleteShader(shaders_[handle]);
  shaders_.erase(handle);
}

// ---------------------------------------------------------------------------
// Pipeline lifecycle
// ---------------------------------------------------------------------------

GlPipelineEntry
OpenGlDevice::buildGraphicsEntry(GLuint program, GLuint vao,
                                 const RhiGraphicsPipelineDesc& desc) {
  GlPipelineEntry e{};
  e.program = program;
  e.vao = vao;
  e.topology = toGlTopology(desc.topology);
  e.vertex_stride = desc.vertex_layout.stride;
  e.loc_u_screen_scale = -1;
  e.blend_enabled = desc.blend.enabled;
  e.depth_test = desc.depth_stencil.depth_test;
  e.depth_write = desc.depth_stencil.depth_write;
  e.wireframe = desc.raster.wireframe;
  e.cull_back = desc.raster.cull_back;
  e.front_ccw = desc.raster.front_ccw;
  return e;
}

RhiPipelineHandle
OpenGlDevice::createGraphicsPipeline(const RhiGraphicsPipelineDesc& desc) {
  auto program = linkProgram(desc.vertex_shader, desc.fragment_shader);
  if (program == 0) {
    return RHI_PIPELINE_INVALID;
  }
  GLuint vao = 0;
  glGenVertexArrays(1, &vao);
  if (desc.vertex_layout.attribute_count > 0) {
    setupVertexLayout(vao, desc.vertex_layout);
  }
  auto handle = allocHandle();
  pipelines_[handle] = buildGraphicsEntry(program, vao, desc);
  return handle;
}

GLuint OpenGlDevice::linkComputeProgram(GLuint shader_id) {
  GLuint program = glCreateProgram();
  glAttachShader(program, shader_id);
  glLinkProgram(program);
  return checkProgramLink(program);
}

RhiPipelineHandle
OpenGlDevice::createComputePipeline(const RhiComputePipelineDesc& desc) {
  if (!shaders_.contains(desc.compute_shader)) {
    return RHI_PIPELINE_INVALID;
  }
  auto program = linkComputeProgram(shaders_[desc.compute_shader]);
  if (program == 0) {
    return RHI_PIPELINE_INVALID;
  }
  auto handle = allocHandle();
  GlPipelineEntry ce{};
  ce.program = program;
  ce.topology = 0;
  ce.blend_enabled = false;
  ce.depth_test = false;
  ce.depth_write = false;
  ce.wireframe = false;
  ce.cull_back = false;
  ce.front_ccw = true;
  pipelines_[handle] = ce;
  return handle;
}

void OpenGlDevice::destroyPipeline(RhiPipelineHandle handle) {
  if (!pipelines_.contains(handle)) {
    return;
  }
  auto& entry = pipelines_[handle];
  glDeleteProgram(entry.program);
  if (entry.vao != 0) {
    glDeleteVertexArrays(1, &entry.vao);
  }
  pipelines_.erase(handle);
}

// ---------------------------------------------------------------------------
// Swap chain
// ---------------------------------------------------------------------------

RhiTextureHandle OpenGlDevice::backbufferTexture() const {
  return backbuffer_handle_;
}

uint32_t OpenGlDevice::backbufferWidth() const {
  return config_.backbuffer_width;
}

uint32_t OpenGlDevice::backbufferHeight() const {
  return config_.backbuffer_height;
}

void OpenGlDevice::resizeSwapchain(uint32_t width, uint32_t height) {
  if (width == 0U || height == 0U) {
    return;
  }
  config_.backbuffer_width = width;
  config_.backbuffer_height = height;
}

// ---------------------------------------------------------------------------
// Frame management
// ---------------------------------------------------------------------------

bool OpenGlDevice::beginFrame() {
  return true;
}

void OpenGlDevice::endFrame() {}

void OpenGlDevice::submit(RhiCommandList& cmd) {
  auto* gl_cmd = dynamic_cast<OpenGlCommandList*>(&cmd);
  if (gl_cmd == nullptr) {
    return;
  }
  for (const auto& command : gl_cmd->commands()) {
    replayCommand(command);
  }
}

bool OpenGlDevice::present() {
  if (window_ != nullptr) {
    SDL_GL_SwapWindow(window_);
  }
  return true;
}

// ---------------------------------------------------------------------------
// Command list creation
// ---------------------------------------------------------------------------

std::unique_ptr<RhiCommandList> OpenGlDevice::createCommandList() {
  return std::make_unique<OpenGlCommandList>();
}

// ---------------------------------------------------------------------------
// Capture API
// ---------------------------------------------------------------------------

std::optional<RhiCaptureResult>
OpenGlDevice::captureFramebuffer(const RhiCaptureRequest& request) {
  glFinish();
  auto w = request.width > 0 ? request.width : config_.backbuffer_width;
  auto h = request.height > 0 ? request.height : config_.backbuffer_height;
  auto raw_pixels = readPixels(w, h);
  flipRows(raw_pixels, w, h);
  auto encoded = encodePixels(raw_pixels, w, h, request);
  if (encoded.empty()) {
    return std::nullopt;
  }
  return RhiCaptureResult{std::move(encoded), w, h};
}

bool OpenGlDevice::captureToFile(const RhiCaptureRequest& request,
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

IRhiRayTracing* OpenGlDevice::rayTracing() {
  return nullptr;
}

// ---------------------------------------------------------------------------
// Synchronization
// ---------------------------------------------------------------------------

void OpenGlDevice::waitIdle() {
  glFinish();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void OpenGlDevice::populateCaps() {
  caps_.backend = RhiBackend::OPEN_GL;
  caps_.ray_tracing_supported = false;
  caps_.compute_supported = hasExtension("GL_ARB_compute_shader");
  const auto* renderer = glGetString(GL_RENDERER);
  if (renderer != nullptr) {
    device_name_ = reinterpret_cast<const char*>(renderer);
  }
  caps_.device_name = device_name_.c_str();
  const auto* version = glGetString(GL_VERSION);
  if (version != nullptr) {
    api_version_ = reinterpret_cast<const char*>(version);
  }
  caps_.api_version = api_version_.c_str();
}

GLenum OpenGlDevice::shaderStageToGl(RhiShaderStage stage) {
  switch (stage) {
    case RhiShaderStage::VERTEX:
      return GL_VERTEX_SHADER;
    case RhiShaderStage::FRAGMENT:
      return GL_FRAGMENT_SHADER;
    case RhiShaderStage::COMPUTE:
      return GL_COMPUTE_SHADER;
  }
  return GL_VERTEX_SHADER;
}

GLuint OpenGlDevice::checkProgramLink(GLuint program) {
  int status = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &status);
  if (status == 0) {
    glDeleteProgram(program);
    return 0;
  }
  return program;
}

GLuint OpenGlDevice::linkProgram(RhiShaderHandle vs, RhiShaderHandle fs) {
  if (!shaders_.contains(vs) || !shaders_.contains(fs)) {
    return 0;
  }
  GLuint program = glCreateProgram();
  glAttachShader(program, shaders_[vs]);
  glAttachShader(program, shaders_[fs]);
  glLinkProgram(program);
  return checkProgramLink(program);
}

void OpenGlDevice::setupVertexLayout(GLuint vao,
                                     const RhiVertexLayout& layout) {
  glBindVertexArray(vao);
  for (uint32_t i = 0; i < layout.attribute_count; ++i) {
    const auto& attr = layout.attributes[i];
    glEnableVertexAttribArray(attr.location);
    glVertexAttribFormat(
        attr.location,
        static_cast<int>(GlFormatInfo::componentCount(attr.format)),
        GlFormatInfo::componentType(attr.format), false, attr.offset);
    glVertexAttribBinding(attr.location, 0);
  }
  glVertexBindingDivisor(0, 0);
  glBindVertexArray(0);
}

void OpenGlDevice::applyPipelineState(const GlPipelineEntry& entry) {
  glUseProgram(entry.program);
  applyBlendDepthState(entry);
  applyRasterState(entry);
  if (entry.vao != 0) {
    glBindVertexArray(entry.vao);
  }
}

void OpenGlDevice::applyBlendDepthState(const GlPipelineEntry& entry) {
  if (entry.blend_enabled) {
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                        GL_ONE_MINUS_SRC_ALPHA);
  } else {
    glDisable(GL_BLEND);
  }
  if (entry.depth_test) {
    glEnable(GL_DEPTH_TEST);
  } else {
    glDisable(GL_DEPTH_TEST);
  }
  glDepthMask(entry.depth_write ? 1 : 0);
}

void OpenGlDevice::applyRasterState(const GlPipelineEntry& entry) {
  if (entry.cull_back) {
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
  } else {
    glDisable(GL_CULL_FACE);
  }
  glFrontFace(entry.front_ccw ? GL_CCW : GL_CW);
  glPolygonMode(GL_FRONT, entry.wireframe ? GL_LINE : GL_FILL);
}

void OpenGlDevice::replayCommand(const GlCommand& command) {
  std::visit([this](const auto& cmd) { executeCommand(cmd); }, command);
}

uint32_t OpenGlDevice::applyClearState(const RhiRenderPassBeginInfo& info) {
  uint32_t bits = 0;
  if (info.color_load_op == RhiLoadOp::CLEAR) {
    const auto& c = info.clear_color;
    glClearColor(c[0], c[1], c[2], c[3]);
    bits |= GL_COLOR_BUFFER_BIT;
  }
  if (info.depth_load_op == RhiLoadOp::CLEAR) {
    glClearDepthf(info.clear_depth);
    bits |= GL_DEPTH_BUFFER_BIT;
  }
  return bits;
}

void OpenGlDevice::executeCommand(const GlCmdBeginRenderPass& cmd) {
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  pass_depth_target_ = cmd.info.depth_target;
  auto clear_bits = applyClearState(cmd.info);
  if (clear_bits != 0) {
    glClear(clear_bits);
  }
}

void OpenGlDevice::executeCommand(const GlCmdEndRenderPass& /*cmd*/) {
  copyPassDepth();
  pass_depth_target_ = RHI_TEXTURE_INVALID;
}

void OpenGlDevice::copyPassDepth() {
  // Every pass draws into the default framebuffer, depth included, whatever
  // depth texture it named: that texture is never attached. A caller that
  // asked to sample it — the outline pass — is handed a copy of what the
  // pass left in the framebuffer's own depth buffer, taken before the next
  // pass can clear it. The copy keeps GL's bottom-up rows, which is what
  // the outline shader expects.
  auto it = textures_.find(pass_depth_target_);
  if (it == textures_.end() || !(it->second.usage & RhiTextureUsage::SAMPLED) ||
      !(it->second.usage & RhiTextureUsage::DEPTH_STENCIL)) {
    return;
  }
  const GlTextureEntry& tex = it->second;
  glBindTexture(GL_TEXTURE_2D, tex.gl_id);
  glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
                      static_cast<GLsizei>(tex.width),
                      static_cast<GLsizei>(tex.height));
  glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGlDevice::executeCommand(const GlCmdBindPipeline& cmd) {
  current_pipeline_ = cmd.pipeline;
  if (pipelines_.contains(cmd.pipeline)) {
    auto& entry = pipelines_[cmd.pipeline];
    current_topology_ = entry.topology;
    applyPipelineState(entry);
  }
}

void OpenGlDevice::executeCommand(const GlCmdBindVertexBuffer& cmd) {
  if (buffers_.contains(cmd.buffer)) {
    auto& entry = buffers_[cmd.buffer];
    GLsizei stride = 0;
    if (pipelines_.contains(current_pipeline_)) {
      stride =
          static_cast<GLsizei>(pipelines_[current_pipeline_].vertex_stride);
    }
    glBindVertexBuffer(0, entry.gl_id, static_cast<int64_t>(cmd.offset),
                       stride);
  }
}

namespace {

  /// The mesh light block's header: a count and the rest of its register.
  constexpr uint32_t MESH_LIGHT_HEADER_BYTES = 16;
  /// How many `vec4`s the light array is: three per light, eight lights.
  constexpr int MESH_LIGHT_VECTORS = 3 * 8;
  /// The whole block, header included, as the shader expects it — see
  /// `MeshRenderer`'s FragmentLights, which is what fills it in.
  constexpr uint32_t MESH_LIGHT_BLOCK_BYTES =
      MESH_LIGHT_HEADER_BYTES +
      (static_cast<uint32_t>(MESH_LIGHT_VECTORS) * 16U);
  /// Where the band count sits in that header: the word after the count.
  constexpr uint32_t MESH_SHADE_BANDS_OFFSET = 4;
  /// How many `vec4`s the outline block is — `OutlineUniforms` in
  /// `mesh-outline-renderer.cpp`.
  constexpr int OUTLINE_VECTORS = 3;
  /// The outline block's size in bytes.
  constexpr uint32_t OUTLINE_BLOCK_BYTES =
      static_cast<uint32_t>(OUTLINE_VECTORS) * 16U;

}  // namespace

void OpenGlDevice::executeCommand(const GlCmdSetVertexStageBytes& cmd) {
  if (cmd.size == 0 || cmd.size > sizeof(cmd.data) || cmd.slot != 1U ||
      !pipelines_.contains(current_pipeline_)) {
    return;
  }
  auto& pe = pipelines_[current_pipeline_];
  const auto* f = reinterpret_cast<const float*>(cmd.data);
  glUseProgram(pe.program);
  // Which uniforms the pipeline has is what says how to read the payload:
  // the mesh program takes two matrices, the GUI program a screen scale.
  // Neither needs to be told which it is.
  if (pe.loc_u_view_projection >= 0 && cmd.size >= sizeof(float) * 32) {
    setMeshMatrices(pe, f);
  } else if (pe.loc_u_screen_scale >= 0 && cmd.size >= sizeof(float) * 2) {
    glUniform2fv(pe.loc_u_screen_scale, 1, f);
  }
}

void OpenGlDevice::setMeshMatrices(const GlPipelineEntry& pe,
                                   const float* matrices) {
  glUniformMatrix4fv(pe.loc_u_view_projection, 1, GL_FALSE, matrices);
  if (pe.loc_u_model >= 0) {
    glUniformMatrix4fv(pe.loc_u_model, 1, GL_FALSE, matrices + 16);
  }
}

void OpenGlDevice::executeCommand(const GlCmdSetFragmentStageBytes& cmd) {
  if (cmd.size == 0 || cmd.size > sizeof(cmd.data) || cmd.slot != 0U ||
      !pipelines_.contains(current_pipeline_)) {
    return;
  }
  const auto& pe = pipelines_[current_pipeline_];
  glUseProgram(pe.program);
  // As on the vertex stage, the uniforms the program resolved say what the
  // payload is: the mesh's lights, or the outline's three vectors.
  if (pe.loc_u_light_count >= 0 && cmd.size >= MESH_LIGHT_BLOCK_BYTES) {
    setMeshLights(pe, cmd.data);
  } else if (pe.loc_u_outline >= 0 && cmd.size >= OUTLINE_BLOCK_BYTES) {
    glUniform4fv(pe.loc_u_outline, OUTLINE_VECTORS,
                 reinterpret_cast<const float*>(cmd.data));
  }
}

void OpenGlDevice::setMeshLights(const GlPipelineEntry& pe,
                                 const uint8_t* block) {
  // A count and a band count in the first register, then the lights — see
  // `MeshRenderer`'s FragmentLights, whose layout this reads by hand
  // because GL has no struct binding short of a uniform buffer.
  uint32_t count = 0;
  std::memcpy(&count, block, sizeof(count));
  glUniform1ui(pe.loc_u_light_count, count);
  uint32_t bands = 0;
  std::memcpy(&bands, block + MESH_SHADE_BANDS_OFFSET, sizeof(bands));
  glUniform1ui(pe.loc_u_shade_bands, bands);
  if (pe.loc_u_lights >= 0) {
    const auto* lights =
        reinterpret_cast<const float*>(block + MESH_LIGHT_HEADER_BYTES);
    glUniform4fv(pe.loc_u_lights, MESH_LIGHT_VECTORS, lights);
  }
}

void OpenGlDevice::executeCommand(const GlCmdBindFragmentTexture& cmd) {
  GLuint gl_tex = 0;
  if (textures_.contains(cmd.texture)) {
    gl_tex = textures_[cmd.texture].gl_id;
  }
  glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(cmd.unit));
  glBindTexture(GL_TEXTURE_2D, gl_tex);
}

void OpenGlDevice::executeCommand(const GlCmdBindIndexBuffer& cmd) {
  current_index_type_ = cmd.index_type;
  if (buffers_.contains(cmd.buffer)) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers_[cmd.buffer].gl_id);
  }
}

void OpenGlDevice::executeCommand(const GlCmdBindDescriptorSet& /*cmd*/) {}

void OpenGlDevice::executeCommand(const GlCmdSetViewport& cmd) {
  glViewport(static_cast<int>(cmd.viewport.x), static_cast<int>(cmd.viewport.y),
             static_cast<int>(cmd.viewport.width),
             static_cast<int>(cmd.viewport.height));
  glDepthRangef(cmd.viewport.min_depth, cmd.viewport.max_depth);
}

void OpenGlDevice::executeCommand(const GlCmdSetScissor& cmd) {
  glEnable(GL_SCISSOR_TEST);
  glScissor(cmd.scissor.x, cmd.scissor.y, static_cast<int>(cmd.scissor.width),
            static_cast<int>(cmd.scissor.height));
}

void OpenGlDevice::executeCommand(const GlCmdDraw& cmd) {
  glDrawArraysInstanced(current_topology_,
                        static_cast<int>(cmd.params.first_vertex),
                        static_cast<int>(cmd.params.vertex_count),
                        static_cast<int>(cmd.params.instance_count));
}

void OpenGlDevice::executeCommand(const GlCmdDrawIndexed& cmd) {
  auto offset_bytes = static_cast<uintptr_t>(cmd.params.first_index) *
                      indexTypeSize(current_index_type_);
  glDrawElementsInstanced(current_topology_,
                          static_cast<int>(cmd.params.index_count),
                          toGlIndexType(current_index_type_),
                          reinterpret_cast<const void*>(offset_bytes),
                          static_cast<int>(cmd.params.instance_count));
}

void OpenGlDevice::executeCommand(const GlCmdDispatch& cmd) {
  glDispatchCompute(cmd.groups_x, cmd.groups_y, cmd.groups_z);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

void OpenGlDevice::executeCommand(const GlCmdCopyBuffer& cmd) {
  if (!buffers_.contains(cmd.params.src) ||
      !buffers_.contains(cmd.params.dst)) {
    return;
  }
  glBindBuffer(GL_COPY_READ_BUFFER, buffers_[cmd.params.src].gl_id);
  glBindBuffer(GL_COPY_WRITE_BUFFER, buffers_[cmd.params.dst].gl_id);
  glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
                      static_cast<int64_t>(cmd.params.src_offset),
                      static_cast<int64_t>(cmd.params.dst_offset),
                      static_cast<int64_t>(cmd.params.size));
  glBindBuffer(GL_COPY_READ_BUFFER, 0);
  glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

void OpenGlDevice::readTextureViaPbo(const GlTextureEntry& tex,
                                     GLuint dst_buffer) {
  GLuint fbo = 0;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D, tex.gl_id, 0);
  auto fmt = GlFormatInfo::fromRhiFormat(tex.format);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, dst_buffer);
  glReadPixels(0, 0, static_cast<int>(tex.width), static_cast<int>(tex.height),
               fmt.format, fmt.type, nullptr);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glDeleteFramebuffers(1, &fbo);
}

void OpenGlDevice::executeCommand(const GlCmdCopyTextureToBuffer& cmd) {
  if (!textures_.contains(cmd.src) || !buffers_.contains(cmd.dst)) {
    return;
  }
  readTextureViaPbo(textures_[cmd.src], buffers_[cmd.dst].gl_id);
}

void OpenGlDevice::executeCommand(const GlCmdTextureBarrier& /*cmd*/) {
  glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

void OpenGlDevice::flipRows(std::vector<uint8_t>& pixels, uint32_t width,
                            uint32_t height) {
  const auto row_bytes = static_cast<size_t>(width) * 4;
  std::vector<uint8_t> row(row_bytes);
  for (uint32_t y = 0; y < height / 2; ++y) {
    auto* top = pixels.data() + y * row_bytes;
    auto* bot = pixels.data() + (height - 1 - y) * row_bytes;
    std::memcpy(row.data(), top, row_bytes);
    std::memcpy(top, bot, row_bytes);
    std::memcpy(bot, row.data(), row_bytes);
  }
}

std::vector<uint8_t> OpenGlDevice::readPixels(uint32_t w, uint32_t h) {
  std::vector<uint8_t> pixels(static_cast<size_t>(w) * h * 4);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glReadPixels(0, 0, static_cast<int>(w), static_cast<int>(h), GL_RGBA,
               GL_UNSIGNED_BYTE, pixels.data());
  return pixels;
}

namespace {

  /// stb callback: append bytes to a vector.
  void stbWriteCallback(void* context, void* data, int size) {
    auto* out = static_cast<std::vector<uint8_t>*>(context);
    const auto* bytes = static_cast<const uint8_t*>(data);
    out->insert(out->end(), bytes, bytes + size);
  }

}  // namespace

std::vector<uint8_t>
OpenGlDevice::encodePixels(const std::vector<uint8_t>& raw, uint32_t w,
                           uint32_t h, const RhiCaptureRequest& request) {
  constexpr int CHANNELS = 4;
  auto stride = static_cast<int>(w) * CHANNELS;
  std::vector<uint8_t> encoded;

  if (request.format == RhiCaptureFormat::JPEG) {
    stbi_write_jpg_to_func(stbWriteCallback, &encoded, static_cast<int>(w),
                           static_cast<int>(h), CHANNELS, raw.data(),
                           static_cast<int>(request.jpeg_quality));
  } else {
    stbi_write_png_to_func(stbWriteCallback, &encoded, static_cast<int>(w),
                           static_cast<int>(h), CHANNELS, raw.data(), stride);
  }
  return encoded;
}

bool OpenGlDevice::hasExtension(std::string_view name) {
  constexpr GLenum GL_NUM_EXTENSIONS_C = 0x821D;
  constexpr GLenum GL_EXTENSIONS_C = 0x1F03;

  int count = 0;
  glGetIntegerv(GL_NUM_EXTENSIONS_C, &count);
  for (int i = 0; i < count; ++i) {
    const auto* ext = glGetStringi(GL_EXTENSIONS_C, static_cast<GLuint>(i));
    if (ext != nullptr && name == reinterpret_cast<const char*>(ext)) {
      return true;
    }
  }
  return false;
}

namespace {

  /// Keep in sync with `sizeof(eng::GuiVertex)` /
  /// `eng::gui::GUI_VERTEX_STRIDE`.
  constexpr uint32_t GUI_VERTEX_STRIDE_BYTES = 40;
  /// Size of one `eng::MeshVertex`: position, normal, and texture
  /// coordinate. Restated here as every backend restates it — see
  /// `mesh-vertex.h`, whose own test asserts this number.
  constexpr uint32_t MESH_VERTEX_STRIDE_BYTES = 32;
  /// Byte offsets of the mesh vertex attributes within that stride.
  constexpr uint32_t MESH_NORMAL_OFFSET = 12;
  constexpr uint32_t MESH_UV_OFFSET = 24;

  /// Mesh shaders, mirroring MESH_MSL_SOURCE in the Metal backend and
  /// MESH_HLSL_SOURCE in the DX12 one function for function: the same
  /// ambient and diffuse split, the same point falloff, the same sRGB
  /// encode on the way out, and the same diffuse map sampled before the
  /// lighting is applied. Three shaders that must agree, in three
  /// languages, none of which can include the C++ header the constants
  /// live in — so `mesh-light.h` is restated here and moves with them.
  constexpr const char MESH_VERTEX_SHADER_GLSL[] = R"glsl(
#version 460 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
uniform mat4 u_view_projection;
uniform mat4 u_model;
out vec3 v_world_position;
out vec3 v_normal;
out vec2 v_uv;
void main() {
  vec4 world = u_model * vec4(a_position, 1.0);
  gl_Position = u_view_projection * world;
  v_world_position = world.xyz;
  // The placement transform is a rotation and a uniform scale, so the same
  // matrix carries the normal; the length the scale adds comes back out in
  // the normalize below.
  v_normal = (u_model * vec4(a_normal, 0.0)).xyz;
  v_uv = a_uv;
}
)glsl";

  constexpr const char MESH_FRAGMENT_SHADER_GLSL[] = R"glsl(
#version 460 core
const uint MESH_MAX_LIGHTS = 8u;
const float MESH_LIGHT_AMBIENT = 0.38;
const float MESH_LIGHT_DIFFUSE = 0.62;
const float MESH_LIGHT_POINT = 1.0;

in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
uniform uint u_light_count;
uniform uint u_shade_bands;
// Three vec4s per light, as `MeshLight` is laid out: position and range,
// direction and intensity, colour and kind.
uniform vec4 u_lights[3 * 8];
layout(binding = 0) uniform sampler2D u_mesh_tex;
out vec4 frag_color;

/// One light's strength flattened into `bands` tones, or left alone for
/// fewer than two. `meshShadeBand` in `mesh-style.h`, restated.
float mesh_band(float light, uint bands) {
  if (bands < 2u) {
    return light;
  }
  float top = float(bands - 1u);
  return min(floor(light * float(bands)), top) / top;
}

float srgb_to_lin(float srgb) {
  if (srgb <= 0.04045) {
    return srgb / 12.92;
  }
  return pow((srgb + 0.055) / 1.055, 2.4);
}

/// How much of a point light reaches a surface this far from it: full at the
/// light, nothing at its range, and squared in between so the falloff reads
/// as light rather than as a gradient.
float mesh_falloff(float dist, float range) {
  if (range <= 0.0) {
    return 0.0;
  }
  float reach = clamp(1.0 - dist / range, 0.0, 1.0);
  return reach * reach;
}

/// What one light adds to a surface.
vec3 mesh_light_contribution(uint index, vec3 world_position, vec3 normal) {
  vec4 position_range = u_lights[index * 3u];
  vec4 direction_intensity = u_lights[index * 3u + 1u];
  vec4 color_kind = u_lights[index * 3u + 2u];
  vec3 to_light = direction_intensity.xyz;
  float attenuation = 1.0;
  if (color_kind.w == MESH_LIGHT_POINT) {
    vec3 offset = position_range.xyz - world_position;
    attenuation = mesh_falloff(length(offset), position_range.w);
    to_light = offset;
  }
  // A light aimed nowhere lights nothing, rather than dividing by zero.
  float aim = length(to_light);
  if (aim < 1e-4 || attenuation <= 0.0) {
    return vec3(0.0);
  }
  float lambert = clamp(dot(normal, to_light / aim), 0.0, 1.0);
  return color_kind.xyz * direction_intensity.w *
         mesh_band(lambert * attenuation, u_shade_bands) * MESH_LIGHT_DIFFUSE;
}

void main() {
  vec3 n = normalize(v_normal);
  vec3 lit = vec3(MESH_LIGHT_AMBIENT);
  uint count = min(u_light_count, MESH_MAX_LIGHTS);
  for (uint i = 0u; i < count; ++i) {
    lit += mesh_light_contribution(i, v_world_position, n);
  }
  // The map is unorm, so this is the sRGB value the artist authored, shaded
  // and then converted on the way out. An instance with no map of its own
  // samples one texel of the flat colour this replaced, so there is no
  // untextured branch here.
  vec3 base = clamp(texture(u_mesh_tex, v_uv).rgb * lit, 0.0, 1.0);
  frag_color = vec4(srgb_to_lin(base.r), srgb_to_lin(base.g),
                    srgb_to_lin(base.b), 1.0);
}
)glsl";

  /// Outline shaders, mirroring OUTLINE_MSL_SOURCE in the Metal backend and
  /// OUTLINE_HLSL_SOURCE in the DX12 one. `mesh-outline-renderer.h` has the
  /// method; `u_outline` is its `OutlineUniforms`, three vec4s.
  constexpr const char OUTLINE_VERTEX_SHADER_GLSL[] = R"glsl(
#version 460 core
void main() {
  // (-1,-1), (3,-1) and (-1,3): one triangle covering all of clip space,
  // so there is no seam down a diagonal and no vertex buffer to bind.
  vec2 corner = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
  gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
)glsl";

  constexpr const char OUTLINE_FRAGMENT_SHADER_GLSL[] = R"glsl(
#version 460 core
// Colour; the scissor's left, top, right and bottom; then the sample width
// and the threshold.
uniform vec4 u_outline[3];
layout(binding = 0) uniform sampler2D u_depth;
out vec4 frag_color;

float outline_depth_at(ivec2 p, ivec2 lo, ivec2 hi) {
  return texelFetch(u_depth, clamp(p, lo, hi), 0).r;
}

void main() {
  // GL counts rows from the bottom, and so does the depth copied out of its
  // framebuffer; only the scissor, which arrives top-down, is turned over.
  int height = textureSize(u_depth, 0).y;
  vec4 bounds = u_outline[1];
  ivec2 lo = ivec2(int(bounds.x), height - int(bounds.w));
  ivec2 hi = ivec2(int(bounds.z), height - int(bounds.y)) - 1;
  ivec2 p = ivec2(gl_FragCoord.xy);
  float centre = outline_depth_at(p, lo, hi);
  // Nothing was drawn here, so there is nothing to outline.
  if (centre >= 1.0) {
    discard;
  }
  int w = int(u_outline[2].x);
  float threshold = u_outline[2].y;
  float across = outline_depth_at(p + ivec2(w, 0), lo, hi) +
                 outline_depth_at(p - ivec2(w, 0), lo, hi) - 2.0 * centre;
  float down = outline_depth_at(p + ivec2(0, w), lo, hi) +
               outline_depth_at(p - ivec2(0, w), lo, hi) - 2.0 * centre;
  // GL's default depth range stores half the clip depth plus a half, so
  // every bend here is half what the threshold was measured against.
  float bend = 2.0 * max(across, down);
  if (bend <= threshold) {
    discard;
  }
  float cover = clamp((bend - threshold) / max(threshold, 1e-9), 0.0, 1.0);
  frag_color = vec4(u_outline[0].rgb, u_outline[0].a * cover);
}
)glsl";

  constexpr const char GUI_VERTEX_SHADER_GLSL[] = R"glsl(
#version 460 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in uint a_packed_color;
layout(location = 3) in float a_corner_radius;
layout(location = 4) in float a_border_width;
layout(location = 5) in uint a_flags;
layout(location = 6) in vec2 a_rect_wh;
uniform vec2 u_screen_scale;
out vec2 v_uv;
out vec4 v_color;
flat out uint v_flags;
flat out float v_corner_radius;
flat out float v_border_width;
flat out vec2 v_rect_wh;
float srgb_to_lin(float srgb) {
  if (srgb <= 0.04045) {
    return srgb / 12.92;
  }
  return pow((srgb + 0.055) / 1.055, 2.4);
}
vec4 unpack_rgba8888(uint c) {
  float r = float(c & 255u) / 255.0;
  float g = float((c >> 8u) & 255u) / 255.0;
  float b = float((c >> 16u) & 255u) / 255.0;
  float a = float((c >> 24u) & 255u) / 255.0;
  return vec4(srgb_to_lin(r), srgb_to_lin(g), srgb_to_lin(b), a);
}
void main() {
  gl_Position = vec4(a_pos.x * u_screen_scale.x - 1.0,
                     1.0 - a_pos.y * u_screen_scale.y, 0.0, 1.0);
  v_uv = a_uv;
  v_color = unpack_rgba8888(a_packed_color);
  v_flags = a_flags;
  v_corner_radius = a_corner_radius;
  v_border_width = a_border_width;
  v_rect_wh = a_rect_wh;
}
)glsl";

  constexpr const char GUI_FRAGMENT_SHADER_GLSL[] = R"glsl(
#version 460 core
in vec2 v_uv;
in vec4 v_color;
flat in uint v_flags;
flat in float v_corner_radius;
flat in float v_border_width;
flat in vec2 v_rect_wh;
layout(binding = 0) uniform sampler2D u_gui_tex;
out vec4 frag_color;
float gui_shape_cover(vec2 p, vec2 rect_wh, float corner_r) {
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
  if ((v_flags & 2u) != 0u) {
    frag_color = texture(u_gui_tex, v_uv) * v_color;
    return;
  }
  vec2 p = vec2((v_uv.x - 0.5) * v_rect_wh.x, (v_uv.y - 0.5) * v_rect_wh.y);
  vec4 c = v_color;
  float bw = v_border_width;
  uint rf = v_flags & 4u;
  if (bw > 1e-5) {
    float cr_o = (rf != 0u) ? v_corner_radius : 0.0;
    float outer_c = gui_shape_cover(p, v_rect_wh, cr_o);
    float irw = max(v_rect_wh.x - 2.0 * bw, 0.0);
    float irh = max(v_rect_wh.y - 2.0 * bw, 0.0);
    float in_r = (rf != 0u) ? max(v_corner_radius - bw, 0.0) : 0.0;
    float inner_c = gui_shape_cover(p, vec2(irw, irh), in_r);
    c.a *= outer_c * (1.0 - inner_c);
    frag_color = c;
    return;
  }
  if (rf != 0u) {
    c.a *= gui_shape_cover(p, v_rect_wh, v_corner_radius);
    frag_color = c;
    return;
  }
  frag_color = c;
}
)glsl";

  /// Vertex array for `eng::MeshVertex`: three float attributes from one
  /// interleaved buffer.
  void setupMeshVertexArray(GLuint vao) {
    glBindVertexArray(vao);
    glEnableVertexAttribArray(0);
    glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexAttribBinding(0, 0);
    glEnableVertexAttribArray(1);
    glVertexAttribFormat(1, 3, GL_FLOAT, GL_FALSE, MESH_NORMAL_OFFSET);
    glVertexAttribBinding(1, 0);
    glEnableVertexAttribArray(2);
    glVertexAttribFormat(2, 2, GL_FLOAT, GL_FALSE, MESH_UV_OFFSET);
    glVertexAttribBinding(2, 0);
    glBindVertexArray(0);
  }

  void setupGuiVertexArray(GLuint vao) {
    glBindVertexArray(vao);
    glEnableVertexAttribArray(0);
    glVertexAttribFormat(0, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexAttribBinding(0, 0);
    glEnableVertexAttribArray(1);
    glVertexAttribFormat(1, 2, GL_FLOAT, GL_FALSE, 8);
    glVertexAttribBinding(1, 0);
    glEnableVertexAttribArray(2);
    glVertexAttribIFormat(2, 1, GL_UNSIGNED_INT, 16);
    glVertexAttribBinding(2, 0);
    glEnableVertexAttribArray(3);
    glVertexAttribFormat(3, 1, GL_FLOAT, GL_FALSE, 20);
    glVertexAttribBinding(3, 0);
    glEnableVertexAttribArray(4);
    glVertexAttribFormat(4, 1, GL_FLOAT, GL_FALSE, 24);
    glVertexAttribBinding(4, 0);
    glEnableVertexAttribArray(5);
    glVertexAttribIFormat(5, 1, GL_UNSIGNED_INT, 28);
    glVertexAttribBinding(5, 0);
    glEnableVertexAttribArray(6);
    glVertexAttribFormat(6, 2, GL_FLOAT, GL_FALSE, 32);
    glVertexAttribBinding(6, 0);
    glVertexBindingDivisor(0, 0);
    glBindVertexArray(0);
  }

}  // namespace

RhiShaderHandle OpenGlDevice::compileStage(const char* glsl,
                                           RhiShaderStage stage) {
  RhiShaderDesc desc{};
  desc.stage = stage;
  desc.bytecode = reinterpret_cast<const uint8_t*>(glsl);
  desc.bytecode_size = std::strlen(glsl);
  return createShader(desc);
}

GLuint OpenGlDevice::linkShaderSource(const char* vertex_glsl,
                                      const char* fragment_glsl) {
  const RhiShaderHandle vs = compileStage(vertex_glsl, RhiShaderStage::VERTEX);
  if (vs == RHI_SHADER_INVALID) {
    return 0;
  }
  const RhiShaderHandle fs =
      compileStage(fragment_glsl, RhiShaderStage::FRAGMENT);
  if (fs == RHI_SHADER_INVALID) {
    destroyShader(vs);
    return 0;
  }
  const GLuint program = linkProgram(vs, fs);
  destroyShader(vs);
  destroyShader(fs);
  return program;
}

namespace {

  /// Fixed function state for the mesh pipeline.
  ///
  /// Opaque geometry, depth-tested, and drawn without culling: OBJ files in
  /// the wild disagree about winding, and showing the geometry beats saving
  /// the fragments. The Metal and DX12 backends make the same call for the
  /// same reason.
  RhiGraphicsPipelineDesc meshPipelineDesc() {
    RhiGraphicsPipelineDesc desc{};
    desc.vertex_layout.stride = MESH_VERTEX_STRIDE_BYTES;
    desc.blend.enabled = false;
    desc.depth_stencil.depth_test = true;
    desc.depth_stencil.depth_write = true;
    desc.raster.cull_back = false;
    desc.color_format = RhiFormat::RGB_A8_SRGB;
    return desc;
  }

  /// Fixed function state for the outline pipeline: blended over the
  /// scene, and neither testing nor writing depth, since the depth it reads
  /// is a texture rather than the framebuffer's.
  RhiGraphicsPipelineDesc outlinePipelineDesc() {
    RhiGraphicsPipelineDesc desc{};
    desc.blend.enabled = true;
    desc.depth_stencil.depth_test = false;
    desc.depth_stencil.depth_write = false;
    desc.raster.cull_back = false;
    desc.color_format = RhiFormat::RGB_A8_SRGB;
    return desc;
  }

  /// A vertex array for a freshly linked program, or zero — with the
  /// program deleted, since nothing will own it — when GL cannot make one.
  GLuint createVertexArrayFor(GLuint program) {
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    if (vao == 0) {
      glDeleteProgram(program);
    }
    return vao;
  }

  /// Look up every uniform the mesh program's stage-bytes payloads land in.
  void resolveMeshUniforms(GlPipelineEntry& entry, GLuint program) {
    entry.loc_u_view_projection =
        glGetUniformLocation(program, "u_view_projection");
    entry.loc_u_model = glGetUniformLocation(program, "u_model");
    entry.loc_u_light_count = glGetUniformLocation(program, "u_light_count");
    entry.loc_u_lights = glGetUniformLocation(program, "u_lights");
    entry.loc_u_shade_bands = glGetUniformLocation(program, "u_shade_bands");
  }

}  // namespace

bool OpenGlDevice::tryCreateMeshOutlinePipeline(
    RhiPipelineHandle& out_pipeline) {
  const GLuint program = linkShaderSource(OUTLINE_VERTEX_SHADER_GLSL,
                                          OUTLINE_FRAGMENT_SHADER_GLSL);
  if (program == 0) {
    return false;
  }
  // A core profile draws nothing with no vertex array bound, even though
  // this one has no attributes: the triangle comes from gl_VertexID.
  const GLuint vao = createVertexArrayFor(program);
  if (vao == 0) {
    return false;
  }
  GlPipelineEntry entry =
      buildGraphicsEntry(program, vao, outlinePipelineDesc());
  entry.loc_u_outline = glGetUniformLocation(program, "u_outline");
  out_pipeline = allocHandle();
  pipelines_[out_pipeline] = entry;
  return true;
}

bool OpenGlDevice::tryCreateMeshPipeline(RhiPipelineHandle& out_pipeline) {
  const GLuint program =
      linkShaderSource(MESH_VERTEX_SHADER_GLSL, MESH_FRAGMENT_SHADER_GLSL);
  if (program == 0) {
    return false;
  }
  const GLuint vao = createVertexArrayFor(program);
  if (vao == 0) {
    return false;
  }
  setupMeshVertexArray(vao);

  GlPipelineEntry entry = buildGraphicsEntry(program, vao, meshPipelineDesc());
  resolveMeshUniforms(entry, program);
  const RhiPipelineHandle handle = allocHandle();
  pipelines_[handle] = entry;
  out_pipeline = handle;
  return true;
}

bool OpenGlDevice::tryCreateGuiPipeline(RhiPipelineHandle& out_pipeline) {
  RhiShaderDesc vs_desc{};
  vs_desc.stage = RhiShaderStage::VERTEX;
  vs_desc.bytecode = reinterpret_cast<const uint8_t*>(GUI_VERTEX_SHADER_GLSL);
  vs_desc.bytecode_size = std::strlen(GUI_VERTEX_SHADER_GLSL);
  RhiShaderHandle vs = createShader(vs_desc);
  if (vs == RHI_SHADER_INVALID) {
    return false;
  }

  RhiShaderDesc fs_desc{};
  fs_desc.stage = RhiShaderStage::FRAGMENT;
  fs_desc.bytecode = reinterpret_cast<const uint8_t*>(GUI_FRAGMENT_SHADER_GLSL);
  fs_desc.bytecode_size = std::strlen(GUI_FRAGMENT_SHADER_GLSL);
  RhiShaderHandle fs = createShader(fs_desc);
  if (fs == RHI_SHADER_INVALID) {
    destroyShader(vs);
    return false;
  }

  GLuint program = linkProgram(vs, fs);
  destroyShader(vs);
  destroyShader(fs);
  if (program == 0) {
    return false;
  }

  GLuint vao = 0;
  glGenVertexArrays(1, &vao);
  if (vao == 0) {
    glDeleteProgram(program);
    return false;
  }
  setupGuiVertexArray(vao);

  RhiGraphicsPipelineDesc gdesc{};
  gdesc.vertex_layout.stride = GUI_VERTEX_STRIDE_BYTES;
  gdesc.blend.enabled = true;
  gdesc.depth_stencil.depth_test = false;
  gdesc.depth_stencil.depth_write = false;
  gdesc.raster.cull_back = false;
  gdesc.color_format = RhiFormat::RGB_A8_SRGB;

  GlPipelineEntry entry = buildGraphicsEntry(program, vao, gdesc);
  entry.loc_u_screen_scale = glGetUniformLocation(program, "u_screen_scale");
  const RhiPipelineHandle handle = allocHandle();
  pipelines_[handle] = entry;
  out_pipeline = handle;
  return true;
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_OPENGL
