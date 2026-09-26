#pragma once

/// @file text-pipeline-fonts.h
/// @brief Opening a font file at a weight — variable axis, collection face
/// or synthetic bold — and the per-face lookups the pipeline needs.
/// @par Threading
/// Main thread only (FreeType faces are not shared).

#include "font-request.h"
#include "opened-font.h"

#include <engine/gui/font-face.h>
#include <engine/gui/glyph-info.h>
#include <engine/gui/text-pipeline.h>
#include <optional>
#include <string_view>
#include <vector>

namespace eng {

/// Open @p path at @p request using @p ft_library (an `FT_Library`): in a
/// collection, the face nearest the weight with the right slant; in a
/// variable font, its `wght` axis set to the weight.
[[nodiscard]] std::optional<OpenedFont>
openFontAtWeight(void* ft_library, std::string_view path,
                 const FontRequest& request);

/// The id of the face in @p faces nearest @p request: the right slant
/// first, then the closest weight.
[[nodiscard]] std::optional<uint32_t>
nearestFace(const std::vector<FontFace>& faces, const FontRequest& request);

/// Kerning between @p left and @p right in @p ft_face (an `FT_Face`), at
/// the size they were rasterized at, in layout pixels.
[[nodiscard]] float fontKerning(void* ft_face, const GlyphInfo& left,
                                const GlyphInfo& right);

}  // namespace eng
