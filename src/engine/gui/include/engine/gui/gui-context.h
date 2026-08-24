#pragma once

/// @file gui-context.h
/// @brief Owns widget tree, theme stack, text pipeline, and GUI renderer;
/// single entry for update/render.
/// @par Threading Main thread only.

#include <cstdint>
#include <engine/gui/gui-rect.h>
#include <engine/gui/gui-renderer.h>
#include <engine/gui/gui-theme.h>
#include <engine/gui/gui-widget-tree.h>
#include <engine/gui/text-pipeline.h>
#include <engine/render/rhi-command-list.h>
#include <engine/render/rhi-device.h>
#include <memory>
#include <string_view>

namespace eng {

/// @thread_safety Main thread only.
class GuiContext {
public:
  /// Owned widget tree context for all GUI widgets.
  std::unique_ptr<GuiWidgetTree> tree{};
  /// Owned theme scope stack for themed rendering.
  std::unique_ptr<ThemeScopeStack> theme_stack{};
  /// Owned text pipeline context for font rendering.
  std::unique_ptr<TextPipelineContext> text_pipeline{};
  /// Owned GUI renderer context for draw submission.
  std::unique_ptr<GuiRendererContext> renderer{};

  /// Layout width in window coordinates (matches SDL mouse / window client
  /// size).
  uint32_t viewport_width = 0;
  /// Layout height in window coordinates.
  uint32_t viewport_height = 0;
  /// Frame delta time in seconds.
  float dt = 0.0f;

  /// True when the developer console overlay is open.
  bool dev_console_open = false;

  /// Owned copy of the application root theme (root_theme_stack pointer targets
  /// this). Empty until a theme is applied successfully.
  std::unique_ptr<Theme> root_theme_storage{};

  ~GuiContext();

  /// Initialise all GUI subsystems. Returns false on failure.
  /// Pass nullptr for device to skip GPU resource creation (unit-test mode).
  bool init(uint32_t width, uint32_t height, RhiDevice* device = nullptr);

  /// Shut down all GUI subsystems and release resources.
  void shutdown();

  /// Update viewport dimensions (e.g. on window resize).
  void resize(uint32_t width, uint32_t height);

  /// Per-frame update: process input, compute layout on dirty subtrees,
  /// update scroll animations, tick cursor blink timers.
  void update(float delta_time);

  /// Render the widget tree: walk tree, emit quads, batch, submit to RHI.
  void render(RhiCommandList& cmd_list);

  /// Load and set the root theme from a JSON file path.
  bool setRootTheme(std::string_view theme_path);

  /// Apply an already-loaded theme as the application root (transfers ownership
  /// into this context). Used when theme JSON is loaded out-of-band (tests,
  /// editor).
  bool applyRootTheme(Theme&& theme);
};

}  // namespace eng
