#pragma once

#include "text-span.h"

#include <string>
#include <vector>

namespace eng {

/// @thread_safety Main thread only.
struct RichText {
  /// Raw UTF-8 text content.
  std::string text{};
  /// Style spans applied over ranges of the text.
  std::vector<TextSpan> spans{};
};

}  // namespace eng
