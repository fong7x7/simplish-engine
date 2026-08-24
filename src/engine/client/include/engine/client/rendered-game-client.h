#pragma once

#include <engine/client/game-client.h>
#include <engine/gui/gui-context.h>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-key-event.h>
#include <engine/gui/gui-mouse-event.h>
#include <engine/gui/gui-scroll-event.h>
#include <engine/gui/gui-widget-tree.h>
#include <engine/render/rhi-device.h>
#include <string_view>

namespace eng::client {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// RenderedGameClient: GameClient + retained GUI + RHI presentation each frame.
//
// Responsibilities:
// - Own GuiContext and wire text pipeline to rhiDevice() after platform init
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

  /// FreeType face ID used for GUI text rendering (0 = default face).
  [[nodiscard]] virtual uint32_t guiTextFaceId() const { return 0; }

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

private:
  /// Retained GUI tree, fonts, and renderer used for in-game overlays.
  GuiContext gui_{};
};

}  // namespace eng::client
