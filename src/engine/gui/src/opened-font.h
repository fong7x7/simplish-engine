#pragma once

/// @file opened-font.h
/// @brief A FreeType face opened for a weight, and whether it had to be
/// thickened.
/// @par Threading
/// Plain data.

namespace eng {

/// A FreeType face opened for a request.
struct OpenedFont {
  /// The `FT_Face`, owned by the caller from here.
  void* ft_face = nullptr;
  /// Whether nothing in the file was heavy enough, so glyphs must be
  /// thickened in software.
  bool synthetic_bold = false;
};

}  // namespace eng
