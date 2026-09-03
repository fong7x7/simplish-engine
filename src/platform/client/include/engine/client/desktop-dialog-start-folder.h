#pragma once

/// @file desktop-dialog-start-folder.h
/// @brief Where a file dialog should open.
/// @par Threading Thread-safe (pure function over its arguments).

#include <string>
#include <string_view>

namespace eng::client {

/// Pick the folder a save dialog starts in.
///
/// Documents is where a person expects their own work to go. It is not
/// guaranteed to exist — a stripped container, a locked-down account, or a
/// platform with no such concept — so the home folder is the fallback, and
/// an empty result means "let the OS decide", which is what SDL does with a
/// null default location.
///
/// Takes the two candidates rather than reading them itself so the choice
/// is testable without a platform to query.
[[nodiscard]] inline std::string chooseDialogStartFolder(const char* documents,
                                                         const char* home) {
  const std::string_view docs_view = documents != nullptr ? documents : "";
  if (!docs_view.empty()) {
    return std::string(docs_view);
  }
  const std::string_view home_view = home != nullptr ? home : "";
  return std::string(home_view);
}

}  // namespace eng::client
