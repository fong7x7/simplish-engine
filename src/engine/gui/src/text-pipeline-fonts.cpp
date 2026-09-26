#include "text-pipeline-fonts.h"

// NOLINTBEGIN(llvm-include-order) — FreeType requires ft2build.h first
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H
#include FT_TRUETYPE_TABLES_H
// NOLINTEND(llvm-include-order)

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

namespace eng {

namespace {

  /// FreeType 26.6 and 16.16 fixed point.
  constexpr float FT_26_6 = 64.0f;
  constexpr FT_Fixed FT_16_16 = 65536;
  /// Weights at and above this are bold; below it, a face is too light.
  constexpr int BOLD_WEIGHT = 600;
  /// A slant mismatch outweighs any weight difference.
  constexpr int SLANT_PENALTY = 1000;
  /// `wght`, as FreeType tags an axis.
  constexpr FT_ULong WEIGHT_AXIS = (static_cast<FT_ULong>('w') << 24U) |
                                   (static_cast<FT_ULong>('g') << 16U) |
                                   (static_cast<FT_ULong>('h') << 8U) |
                                   static_cast<FT_ULong>('t');

  /// @p face's weight: its OS/2 class, else 700 for a bold style, else 400.
  int faceWeight(FT_Face face) {
    const auto* os2 =
        static_cast<const TT_OS2*>(FT_Get_Sfnt_Table(face, FT_SFNT_OS2));
    if (os2 != nullptr && os2->usWeightClass != 0) {
      return os2->usWeightClass;
    }
    return (face->style_flags & FT_STYLE_FLAG_BOLD) != 0 ? 700 : 400;
  }

  bool isItalic(FT_Face face) {
    return (face->style_flags & FT_STYLE_FLAG_ITALIC) != 0;
  }

  /// How far @p face is from @p request; lower is closer.
  int distance(FT_Face face, const FontRequest& request) {
    const bool want_italic = request.italic == FontLoadItalic::ITALIC;
    return std::abs(faceWeight(face) - request.weight) +
           (isItalic(face) == want_italic ? 0 : SLANT_PENALTY);
  }

  /// Face @p index of @p path, or null.
  FT_Face openIndex(FT_Library lib, const std::string& path, FT_Long index) {
    FT_Face face = nullptr;
    return FT_New_Face(lib, path.c_str(), index, &face) == 0 ? face : nullptr;
  }

  /// The face of @p path's collection nearest @p request.
  FT_Face openNearestInCollection(FT_Library lib, const std::string& path,
                                  const FontRequest& request) {
    FT_Face best = openIndex(lib, path, 0);
    if (best == nullptr) {
      return nullptr;
    }
    for (FT_Long i = 1; i < best->num_faces; ++i) {
      FT_Face next = openIndex(lib, path, i);
      if (next != nullptr &&
          distance(next, request) < distance(best, request)) {
        FT_Done_Face(best);
        best = next;
      } else if (next != nullptr) {
        FT_Done_Face(next);
      }
    }
    return best;
  }

  /// Every axis of @p mm at its default and `wght` at @p weight, or
  /// nothing when it has no `wght` axis.
  std::optional<std::vector<FT_Fixed>> weightCoords(const FT_MM_Var& mm,
                                                    uint16_t weight) {
    std::vector<FT_Fixed> coords(mm.num_axis);
    bool found = false;
    for (FT_UInt i = 0; i < mm.num_axis; ++i) {
      const FT_Var_Axis& axis = mm.axis[i];
      coords[i] = axis.def;
      if (axis.tag == WEIGHT_AXIS) {
        coords[i] = std::clamp(static_cast<FT_Fixed>(weight) * FT_16_16,
                               axis.minimum, axis.maximum);
        found = true;
      }
    }
    return found ? std::optional(coords) : std::nullopt;
  }

  /// Set @p face's `wght` axis, if it has one, to @p weight. True if it did.
  bool setVariableWeight(FT_Library lib, FT_Face face, uint16_t weight) {
    FT_MM_Var* mm = nullptr;
    if (!FT_HAS_MULTIPLE_MASTERS(face) || FT_Get_MM_Var(face, &mm) != 0) {
      return false;
    }
    std::optional<std::vector<FT_Fixed>> coords = weightCoords(*mm, weight);
    if (coords) {
      FT_Set_Var_Design_Coordinates(face, mm->num_axis, coords->data());
    }
    FT_Done_MM_Var(lib, mm);
    return coords.has_value();
  }

}  // namespace

std::optional<OpenedFont> openFontAtWeight(void* ft_library,
                                           std::string_view path,
                                           const FontRequest& request) {
  auto* lib = static_cast<FT_Library>(ft_library);
  FT_Face face = openNearestInCollection(lib, std::string(path), request);
  if (face == nullptr) {
    return std::nullopt;
  }
  const bool variable = setVariableWeight(lib, face, request.weight);
  const bool too_light =
      request.weight >= BOLD_WEIGHT && faceWeight(face) < BOLD_WEIGHT - 50;
  return OpenedFont{face, !variable && too_light};
}

std::optional<uint32_t> nearestFace(const std::vector<FontFace>& faces,
                                    const FontRequest& request) {
  std::optional<uint32_t> best;
  int best_distance = 0;
  for (const FontFace& face : faces) {
    const bool want_italic = request.italic == FontLoadItalic::ITALIC;
    const int d = std::abs(static_cast<int>(face.weight) - request.weight) +
                  (face.italic == want_italic ? 0 : SLANT_PENALTY);
    if (!best || d < best_distance) {
      best = face.face_id;
      best_distance = d;
    }
  }
  return best;
}

float fontKerning(void* ft_face, const GlyphInfo& left,
                  const GlyphInfo& right) {
  auto* face = static_cast<FT_Face>(ft_face);
  if (!FT_HAS_KERNING(face) || left.raster_px == 0) {
    return 0.0f;
  }
  FT_Set_Pixel_Sizes(face, 0, left.raster_px);
  FT_Vector delta{};
  if (FT_Get_Kerning(face, left.glyph_index, right.glyph_index,
                     FT_KERNING_DEFAULT, &delta) != 0) {
    return 0.0f;
  }
  return static_cast<float>(delta.x) / FT_26_6 * left.atlas_layout_scale;
}

}  // namespace eng
