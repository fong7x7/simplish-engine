#include <algorithm>
#include <cstdint>
#include <editor/shell/editor-ground-atlas.h>
#include <editor/shell/editor-terrains.h>
#include <engine/render-ground/ground-mesh.h>

namespace eng::editor {

namespace {

  /// A well-mixed 32-bit hash of a texel's place, from the finaliser of
  /// MurmurHash3: cheap, and with no visible pattern at swatch size.
  uint32_t mix(uint32_t value) {
    value ^= value >> 16;
    value *= 0x85EBCA6BU;
    value ^= value >> 13;
    value *= 0xC2B2AE35U;
    return value ^ (value >> 16);
  }

  /// How far texel (@p x, @p y) of swatch @p terrain strays, from −@p grain
  /// to +@p grain.
  int speckle(uint32_t x, uint32_t y, uint32_t terrain, uint8_t grain) {
    const uint32_t hashed = mix(x + y * GROUND_SWATCH_TEXELS + terrain * 7919U);
    const auto span = static_cast<uint32_t>(grain) * 2U + 1U;
    return static_cast<int>(hashed % span) - static_cast<int>(grain);
  }

  /// @p channel moved by @p offset, held to a byte.
  uint8_t shifted(uint8_t channel, int offset) {
    return static_cast<uint8_t>(std::clamp(channel + offset, 0, 255));
  }

  /// Fill texel (@p x, @p y) of swatch @p index into @p pixels.
  void paintTexel(std::vector<uint8_t>& pixels, uint32_t x, uint32_t y,
                  uint32_t index) {
    const EditorTerrain& terrain = EDITOR_TERRAINS[index];
    const int offset = speckle(x, y, index, terrain.grain);
    const size_t row = static_cast<size_t>(index) * GROUND_SWATCH_TEXELS + y;
    const size_t at = (row * GROUND_SWATCH_TEXELS + x) * 4;
    pixels[at] = shifted(terrain.red, offset);
    pixels[at + 1] = shifted(terrain.green, offset);
    pixels[at + 2] = shifted(terrain.blue, offset);
    pixels[at + 3] = 255;
  }

}  // namespace

ImageData makeEditorGroundAtlas() {
  ImageData atlas;
  atlas.width = GROUND_SWATCH_TEXELS;
  atlas.height =
      GROUND_SWATCH_TEXELS * static_cast<uint32_t>(EDITOR_TERRAIN_COUNT);
  atlas.source_channels = 4;
  atlas.pixels.assign(static_cast<size_t>(atlas.width) * atlas.height * 4, 0);
  for (uint32_t index = 0; index < EDITOR_TERRAIN_COUNT; ++index) {
    for (uint32_t y = 0; y < GROUND_SWATCH_TEXELS; ++y) {
      for (uint32_t x = 0; x < GROUND_SWATCH_TEXELS; ++x) {
        paintTexel(atlas.pixels, x, y, index);
      }
    }
  }
  return atlas;
}

}  // namespace eng::editor
