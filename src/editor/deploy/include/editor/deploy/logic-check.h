#pragma once

/// @file logic-check.h
/// @brief A freshly built game logic library, run in a process of its own.
/// @par Threading Main-thread-only.

#include <editor/deploy/logic-check-options.h>
#include <optional>
#include <ostream>
#include <span>
#include <string_view>

namespace eng::editor {

/// Load @p options' library and run a stand-in game of its content with
/// it, saying what happened to @p out. 0 when it ran its ticks — or its run
/// ended — without a problem; 1, with an `error:` line, when the library
/// would not load or the content would not run.
///
/// Meant to run in `simplish-logic-check`, never the editor: what it
/// guards against is the logic crashing or hanging, which would otherwise
/// take the editor down with it (ADR-011). Its process exiting abnormally
/// is the failure the editor watches for.
[[nodiscard]] int runLogicCheck(const LogicCheckOptions& options,
                                std::ostream& out);

/// The options `simplish-logic-check`'s arguments — program name excluded
/// — ask for: `--library PATH --content DIR [--ticks N]`. Nothing when one
/// is not understood, or the library or content is missing.
[[nodiscard]] std::optional<LogicCheckOptions>
parseLogicCheckArgs(std::span<const std::string_view> args);

}  // namespace eng::editor
