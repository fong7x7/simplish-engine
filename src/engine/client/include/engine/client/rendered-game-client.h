#pragma once

#include <engine/client/game-client.h>
#include <engine/gui/gui-color.h>
#include <engine/gui/gui-context.h>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-key-event.h>
#include <engine/gui/gui-mouse-event.h>
#include <engine/gui/gui-scroll-event.h>
#include <engine/gui/gui-widget-tree.h>
#include <engine/render/rhi-device.h>
#include <engine/render/rhi-render-pass-begin-info.h>
#include <filesystem>
#include <string_view>

namespace eng::client {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// RenderedGameClient: GameClient + retained GUI + RHI presentation each frame.
//
// Responsibilities:
// - Own GuiContext and wire text pipeline to rhiDevice() after platform init
// - Record an optional depth-tested scene pass before the GUI pass, so 3D
//   geometry draws under the interface rather than over it
// - Load the UI font and expose its face id, so GUI text draws as glyphs
//   rather than the placeholder boxes drawn when no face is loaded
// - Present GuiRenderer batches to the swapchain (clear + endFrame + submit)
//
// Input: platform code (e.g. DesktopGameClient) maps native events to the
// guiDispatch* methods; this type has no SDL dependency.
//
// Thread Safety:
// - Main thread only.
// ============================================================================

class RenderedGameClient : public GameClient {
public:
  RenderedGameClient() = default;
  ~RenderedGameClient() override = default;

  /// Present retained-mode GUI to the swapchain. Called from DesktopGameClient
  /// after each onTick; may be invoked manually if the platform loop differs.
  void presentGuiFrame();

protected:
  bool onInit() override;
  void onShutdown() override;

  /// FreeType face id used for GUI text rendering.
  ///
  /// Zero means no face is loaded, and every `drawText` falls back to
  /// placeholder boxes. Loaded ids start at 1, so returning a literal 0 can
  /// never match a face — override this only to point at a face you loaded
  /// into the same text pipeline.
  [[nodiscard]] virtual uint32_t guiTextFaceId() const {
    return gui_text_face_id_;
  }

  /// Platform-specific RHI device for GPU submission (non-null after init).
  [[nodiscard]] virtual eng::RhiDevice* rhiDevice() const = 0;

  /// Mutable access to the retained GUI context owned by this client.
  [[nodiscard]] GuiContext& guiContext() { return gui_; }

  /// The retained widget tree used for GUI layout and input dispatch.
  [[nodiscard]] GuiWidgetTree& guiWidgetTree() const { return *gui_.tree; }

  /// Build a draw context from the current GUI state.
  [[nodiscard]] eng::GuiDrawContext guiDrawContext() const;

  /// Resize retained GUI to current `backbufferWidth` / `backbufferHeight`.
  void resizeGuiToBackbuffer();

  /// GUI layout width in window coordinates (defaults to backbuffer width).
  [[nodiscard]] virtual uint32_t guiLayoutWidth() const {
    return backbufferWidth();
  }

  /// GUI layout height in window coordinates (defaults to backbuffer height).
  [[nodiscard]] virtual uint32_t guiLayoutHeight() const {
    return backbufferHeight();
  }

  /// Forward a mouse-down event to the GUI widget tree.
  void guiDispatchMouseDown(const eng::GuiMouseEvent& event) const;

  /// Forward a mouse-up event to the GUI widget tree.
  void guiDispatchMouseUp(const eng::GuiMouseEvent& event) const;

  /// Forward a mouse-move event to the GUI widget tree.
  void guiDispatchMouseMove(const eng::GuiMouseEvent& event) const;

  /// Forward a scroll event to the GUI widget tree.
  void guiDispatchScroll(const eng::GuiScrollEvent& event) const;

  /// Forward text input to the focused widget; returns true if consumed.
  bool guiDispatchText(std::string_view text) const;

  /// Forward a keycode press to the focused widget; returns true if consumed.
  bool guiDispatchKey(uint32_t keycode) const;

  /// Forward a key event to the focused widget; returns true if consumed.
  bool guiDispatchKey(const eng::GuiKeyEvent& event) const;

  /// Copy RHI pixel surface size into `gui_.renderer` for HiDPI drawing.
  void syncGuiRendererSurfaceFromDevice();

  /// FreeType raster supersample factor (e.g. `SDL_GetWindowPixelDensity`).
  [[nodiscard]] virtual float textRasterSupersample() const { return 1.0f; }

  /// Colour the frame is cleared to, and therefore what shows wherever no
  /// widget paints.
  ///
  /// Whichever pass clears uses this, so a frame with a scene pass and a
  /// frame without one come out identical everywhere the scene draws
  /// nothing. A widget that paints its own opaque background over the
  /// scene's region would erase the scene, since the GUI pass runs second.
  [[nodiscard]] virtual GuiColor frameClearColor() const {
    return {30, 30, 34, 255};
  }

  /// Depth target for the scene pass, or invalid to draw no scene at all.
  ///
  /// Returning a valid handle turns on a render pass before the GUI: colour
  /// and depth are cleared there, `recordScene` draws into it, and the GUI
  /// pass then loads the result instead of clearing it.
  [[nodiscard]] virtual RhiTextureHandle sceneDepthTarget() {
    return RHI_TEXTURE_INVALID;
  }

  /// Record scene draws. Called inside the scene pass, never outside one.
  virtual void recordScene(RhiCommandList& /*cmd*/) {}

  /// Record draws that read what the scene pass wrote — its depth, for the
  /// outline. Called once that pass has ended, at the start of the pass the
  /// GUI then draws over the scene in, which has the same colour target and
  /// no depth attachment. Viewport and scissor are the caller's to set; the
  /// GUI sets its own afterwards.
  virtual void recordSceneOverlay(RhiCommandList& /*cmd*/) {}

  /// Directory searched for a bundled UI font before the system paths.
  /// Defaults to `<data_dir>/fonts` from the engine config.
  [[nodiscard]] virtual std::filesystem::path guiFontDirectory();

private:
  /// Acquire, record, submit, and present one frame.
  void submitFrame(RhiDevice& device);

  /// Record the whole frame: the optional scene pass, then the GUI pass.
  void recordFrame(RhiCommandList& cmd, RhiDevice& device);

  /// Record a frame with a scene in it: GUI under, scene, GUI over.
  void recordLayeredFrame(RhiCommandList& cmd, RhiDevice& device,
                          RhiTextureHandle depth);

  /// Begin the pass 3D geometry draws into, clearing colour and depth.
  void beginScenePass(RhiCommandList& cmd, RhiDevice& device,
                      RhiTextureHandle depth);

  /// Begin the pass the GUI draws into. Loads rather than clears when a
  /// scene pass already painted the frame.
  void beginGuiPass(RhiCommandList& cmd, RhiDevice& device,
                    RhiLoadOp color_load);

  /// Discover, load, and size the UI font. Logs and leaves the face id at
  /// zero when the machine has no usable font.
  void loadGuiFont();

  /// Retained GUI tree, fonts, and renderer used for in-game overlays.
  GuiContext gui_{};
  /// Face id returned by `guiTextFaceId`; zero until a font loads.
  uint32_t gui_text_face_id_ = 0;
};

}  // namespace eng::client
