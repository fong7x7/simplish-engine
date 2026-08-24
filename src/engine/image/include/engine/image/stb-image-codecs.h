#pragma once

/// @file stb-image-codecs.h
/// @brief The single home for the stb_image / stb_image_write implementations.
///
/// stb ships as header-only libraries whose implementations are emitted by
/// defining `STB_IMAGE_IMPLEMENTATION` / `STB_IMAGE_WRITE_IMPLEMENTATION` in
/// exactly one translation unit. This module owns those two translation
/// units for the whole project.
///
/// Both the GUI (image loading, PNG capture of the software rasterizer) and
/// the RHI backends (the capture API) call into stb. Backends live in
/// src/platform/ and the GUI lives in src/engine/, so neither can own the
/// implementation without the other losing the symbol at link time. Defining
/// it per-backend — the arrangement this replaced — worked only because a
/// single backend compiles at a time, and it broke the moment the GUI needed
/// the same symbol.
///
/// Consumers include `<stb_image.h>` / `<stb_image_write.h>` directly for the
/// declarations and link `simplish-engine-image` for the definitions. This
/// header exists to document that contract; it declares nothing itself.
///
/// @par Threading stb entry points are re-entrant; the callers are
/// main-thread-only.

namespace eng::image {}  // namespace eng::image
