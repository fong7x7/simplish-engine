#pragma once

#ifdef ENGINE_RENDERER_OPENGL

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// OpenGlDevice: OpenGL 4.6 fallback backend for the RHI abstraction.
//
// Responsibilities:
// - Create and manage an OpenGL 4.6 Core context via SDL
// - Implement all RhiDevice resource lifecycle methods (buffer, texture,
//   shader, pipeline) by mapping RHI handles to GL object names
// - Replay recorded OpenGlCommandList commands as actual GL calls on submit
// - Provide framebuffer capture via glReadPixels + stb_image_write
// - Manage swap chain via SDL_GL_SwapWindow
//
// Key Invariants:
// - Ray tracing is never supported; rayTracing() always returns nullptr
// - Compute is supported only if GL_ARB_compute_shader is present
// - All GL calls happen on the main thread (context is single-threaded)
// - Resource handles are monotonically increasing uint64_t; 0 = invalid
// - destroy*() is safe to call with invalid handles (no-op)
// - beginFrame() always returns true (no swapchain acquire in OpenGL)
// - present() always returns true (no swapchain loss in OpenGL)
//
// Threading:
// - All methods are main-thread-only (OpenGL context is single-threaded)
// ============================================================================

#include <cstdint>
#include <engine/render/backends/opengl/gl-buffer-entry.h>
#include <engine/render/backends/opengl/gl-pipeline-entry.h>
#include <engine/render/backends/opengl/gl-texture-entry.h>
#include <engine/render/backends/opengl/opengl-command.h>
#include <engine/render/backends/opengl/opengl-types.h>
#include <engine/render/render-config.h>
#include <engine/render/rhi-capture-request.h>
#include <engine/render/rhi-device-capabilities.h>
#include <engine/render/rhi-device.h>
#include <engine/render/rhi-types.h>
#include <engine/render/rhi-vertex-layout.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Forward-declare SDL type to avoid pulling in SDL headers.
struct SDL_Window;
using GLuint = unsigned int;
using GLint = int;

namespace eng {
struct RhiGraphicsPipelineDesc;
}  // namespace eng

namespace eng::render {

class OpenGlDevice final : public RhiDevice {
public:
  /// Attempt to create an OpenGL 4.6 device from the given config.
  /// Returns nullopt if context creation or function loading fails.
  static std::optional<std::unique_ptr<OpenGlDevice>>
  tryCreate(const RenderConfig& config);

  ~OpenGlDevice() override;

  // --- Identification ---
  RhiBackend backend() const override;
  const RhiDeviceCapabilities& capabilities() const override;

  // --- Resource lifecycle ---
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

  bool tryCreateGuiPipeline(RhiPipelineHandle& out_pipeline) override;
  bool tryCreateMeshPipeline(RhiPipelineHandle& out_pipeline) override;
  bool tryCreateMeshOutlinePipeline(RhiPipelineHandle& out_pipeline) override;

  // --- Swap chain ---
  RhiTextureHandle backbufferTexture() const override;
  uint32_t backbufferWidth() const override;
  uint32_t backbufferHeight() const override;
  void resizeSwapchain(uint32_t width, uint32_t height) override;

  // --- Frame management ---
  bool beginFrame() override;
  void endFrame() override;
  void submit(RhiCommandList& cmd) override;
  bool present() override;

  // --- Command list creation ---
  std::unique_ptr<RhiCommandList> createCommandList() override;

  // --- Capture API ---
  std::optional<RhiCaptureResult>
  captureFramebuffer(const RhiCaptureRequest& request) override;
  bool captureToFile(const RhiCaptureRequest& request,
                     std::string_view path) override;

  // --- Extensions ---
  IRhiRayTracing* rayTracing() override;

  // --- Synchronization ---
  void waitIdle() override;

private:
  explicit OpenGlDevice(const RenderConfig& config);

  /// Allocate the next monotonic resource handle.
  RhiBufferHandle allocHandle();

  /// Populate device capabilities from GL queries.
  void populateCaps();

  /// Configure SDL GL attributes for a 4.6 Core context.
  static void setGlAttributes();

  /// Create GL context and load GL functions. Returns null on failure.
  static void* createGlContext(SDL_Window* window);

  /// Initialise device state after successful context creation.
  void initFromContext(void* gl_ctx);

  /// Allocate and initialise a GL buffer with storage.
  static GLuint createGlBuffer(const RhiBufferDesc& desc);

  /// Allocate a GL texture with storage.
  static GLuint createGlTexture(const RhiTextureDesc& desc);

  /// Upload initial pixel data to a bound GL_TEXTURE_2D.
  static void uploadTexturePixels(const RhiTextureDesc& desc,
                                  const GlFormatInfo& fmt);

  /// Map a GL buffer for persistent read/write access.
  static void* mapGlBuffer(GlBufferEntry& entry);

  /// Compile GLSL source into a GL shader object. Returns 0 on failure.
  static GLuint compileGlShader(const RhiShaderDesc& desc);

  /// Check GL shader compile status; deletes shader and returns 0 on failure.
  static GLuint checkShaderCompile(GLuint shader);

  /// Convert RhiShaderStage to GL shader type enum.
  static unsigned int shaderStageToGl(RhiShaderStage stage);

  /// Link vertex + fragment shaders into a GL program.
  GLuint linkProgram(RhiShaderHandle vs, RhiShaderHandle fs);

  /// Check GL program link status; deletes program and returns 0 on failure.
  static GLuint checkProgramLink(GLuint program);

  /// Link a single compute shader into a GL program.
  GLuint linkComputeProgram(GLuint shader_id);

  /// Build a GlPipelineEntry from a graphics pipeline description.
  static GlPipelineEntry
  buildGraphicsEntry(GLuint program, GLuint vao,
                     const RhiGraphicsPipelineDesc& desc);

  /// Configure VAO vertex attributes from RhiVertexLayout.
  static void setupVertexLayout(GLuint vao, const RhiVertexLayout& layout);

  /// Apply GL state from a pipeline entry.
  static void applyPipelineState(const GlPipelineEntry& entry);

  /// Apply GL blend and depth state from a pipeline entry.
  static void applyBlendDepthState(const GlPipelineEntry& entry);

  /// Apply GL rasterizer state from a pipeline entry.
  static void applyRasterState(const GlPipelineEntry& entry);

  /// Set GL clear state and return the bitmask of buffers to clear.
  static uint32_t applyClearState(const RhiRenderPassBeginInfo& info);

  /// Replay a single recorded command via GL calls.
  void replayCommand(const GlCommand& command);

  /// Execute individual command types.
  void executeCommand(const GlCmdBeginRenderPass& cmd);
  void executeCommand(const GlCmdEndRenderPass& cmd);
  void executeCommand(const GlCmdBindPipeline& cmd);
  void executeCommand(const GlCmdBindVertexBuffer& cmd);
  void executeCommand(const GlCmdBindIndexBuffer& cmd);
  /// Compile one GLSL stage. `RHI_SHADER_INVALID` on failure.
  RhiShaderHandle compileStage(const char* glsl, RhiShaderStage stage);

  /// Compile and link one GLSL vertex/fragment pair. Zero on failure.
  GLuint linkShaderSource(const char* vertex_glsl, const char* fragment_glsl);

  /// Push the mesh program's two matrices, read from one payload.
  static void setMeshMatrices(const GlPipelineEntry& pe, const float* matrices);

  /// Push the mesh program's light count, band count, and light array from
  /// one block.
  static void setMeshLights(const GlPipelineEntry& pe, const uint8_t* block);

  /// Copy the default framebuffer's depth into the texture the pass that
  /// just ended named as its depth target, when that texture is sampled.
  void copyPassDepth();

  void executeCommand(const GlCmdSetVertexStageBytes& cmd);
  void executeCommand(const GlCmdSetFragmentStageBytes& cmd);
  void executeCommand(const GlCmdBindFragmentTexture& cmd);
  void executeCommand(const GlCmdBindDescriptorSet& cmd);
  void executeCommand(const GlCmdSetViewport& cmd);
  void executeCommand(const GlCmdSetScissor& cmd);
  void executeCommand(const GlCmdDraw& cmd);
  void executeCommand(const GlCmdDrawIndexed& cmd);
  void executeCommand(const GlCmdDispatch& cmd);
  void executeCommand(const GlCmdCopyBuffer& cmd);
  void executeCommand(const GlCmdCopyTextureToBuffer& cmd);
  void executeCommand(const GlCmdTextureBarrier& cmd);

  /// Read texture pixels via a temporary FBO into a PBO.
  static void readTextureViaPbo(const GlTextureEntry& tex, GLuint dst_buffer);

  /// Flip pixel rows vertically (GL origin is bottom-left).
  static void flipRows(std::vector<uint8_t>& pixels, uint32_t width,
                       uint32_t height);

  /// Read raw RGBA pixels from the default framebuffer.
  static std::vector<uint8_t> readPixels(uint32_t w, uint32_t h);

  /// Encode raw RGBA pixels to PNG or JPEG via stb_image_write.
  static std::vector<uint8_t> encodePixels(const std::vector<uint8_t>& raw,
                                           uint32_t w, uint32_t h,
                                           const RhiCaptureRequest& request);

  /// Query whether an OpenGL extension is present by name.
  static bool hasExtension(std::string_view name);

  /// SDL-owned OpenGL context handle.
  void* gl_context_ = nullptr;
  /// SDL window for swap operations.
  SDL_Window* window_ = nullptr;
  /// Render config snapshot.
  RenderConfig config_{};
  /// Device capabilities populated at init.
  RhiDeviceCapabilities caps_{};
  /// Device name string (owned; caps_.device_name points here).
  std::string device_name_{};
  /// API version string (owned; caps_.api_version points here).
  std::string api_version_{};
  /// Monotonic handle counter (0 = invalid; starts at 1).
  uint64_t next_handle_ = 1;
  /// Buffer resources.
  std::unordered_map<uint64_t, GlBufferEntry> buffers_{};
  /// Texture resources.
  std::unordered_map<uint64_t, GlTextureEntry> textures_{};
  /// Shader programs (individual compiled shaders).
  std::unordered_map<uint64_t, GLuint> shaders_{};
  /// Pipeline entries (linked programs + state).
  std::unordered_map<uint64_t, GlPipelineEntry> pipelines_{};
  /// RHI handle for the default framebuffer (backbuffer).
  RhiTextureHandle backbuffer_handle_ = 0;
  /// Currently bound pipeline topology (for draw calls).
  unsigned int current_topology_ = 0;
  /// Currently bound index type (for indexed draw calls).
  RhiIndexType current_index_type_ = RhiIndexType::UINT16;
  /// Last bound graphics pipeline (for stride / uniform replay).
  RhiPipelineHandle current_pipeline_ = RHI_PIPELINE_INVALID;
  /// Depth target the open render pass named, copied into at its end.
  RhiTextureHandle pass_depth_target_ = RHI_TEXTURE_INVALID;
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_OPENGL
