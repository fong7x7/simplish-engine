#pragma once

/// @file editor-ground-atlas.h
/// @brief The texture the painted ground is drawn with.
/// @par Threading Thread-safe (pure function producing a new image).

#include <engine/gui/image-data.h>

namespace eng::editor {

/// One swatch per terrain in `EDITOR_TERRAINS`, stacked top to bottom in
/// terrain order, each `GROUND_SWATCH_TEXELS` square: the atlas
/// `makeGroundMesh` addresses.
///
/// Each swatch is its terrain's colour, speckled by its grain. The speckle
/// is a hash of the texel's place in the swatch, so it is the same on every
/// run and tiles by construction: a cell maps the whole swatch, and the
/// cell beside it starts the swatch again without a seam.
[[nodiscard]] ImageData makeEditorGroundAtlas();

}  // namespace eng::editor
