#pragma once

/// @file capture-font.h
/// @brief Test support: a text pipeline with the machine's UI font loaded,
/// so software-rasterized captures show legible text instead of the
/// placeholder boxes drawn when no face is available.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <engine/gui/gui-software-rasterizer.h>
#include <engine/gui/text-pipeline.h>

namespace eng::editor::test {

/// Owns a `TextPipelineContext` carrying the UI font.
///
/// `face_id` stays zero when the machine has no usable font, which degrades
/// a capture to placeholder boxes rather than failing it — the captures
/// assert on panel colours, not on glyphs.
/// @thread_safety Main-thread-only.
struct CaptureFont {
  /// The pipeline glyphs are rasterized into.
  eng::TextPipelineContext pipeline{};
  /// Loaded face, or zero when no font was found.
  uint32_t face_id = 0;

  /// Initialise the pipeline and load the UI font, if there is one.
  CaptureFont();

  ~CaptureFont() { pipeline.shutdown(); }
  CaptureFont(const CaptureFont&) = delete;
  CaptureFont& operator=(const CaptureFont&) = delete;
  CaptureFont(CaptureFont&&) = delete;
  CaptureFont& operator=(CaptureFont&&) = delete;

  /// View of the CPU atlas the glyph quads were packed into. Call after
  /// rendering: rasterizing a glyph can reallocate the pixel buffer.
  [[nodiscard]] eng::GuiSoftwareRasterizer::GlyphAtlas atlas() const {
    const auto& page = pipeline.atlases.at(0);
    return {page.rgba_pixels, page.width, page.height};
  }
};

}  // namespace eng::editor::test
