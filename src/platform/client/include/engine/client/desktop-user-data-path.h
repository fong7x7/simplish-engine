#pragma once

/// @file desktop-user-data-path.h
/// @brief Where this machine's user can keep application data.
/// @par Threading Main-thread only.

#include <filesystem>
#include <string_view>

namespace eng::client {

/// The directory this user can write @p app's own data to, created if it is
/// not there yet.
///
///   macOS    ~/Library/Application Support/<org>/<app>/
///   Windows  %APPDATA%\<org>\<app>\
///   Linux    ~/.local/share/<org>/<app>/
///
/// This is not the install directory. An installed application has no
/// business writing beside its own binary — the location may be read-only,
/// it is shared between users, and anything written there is mixed in with
/// the files that were shipped.
///
/// Empty when the platform offers no such place, which the caller has to
/// answer for rather than writing somewhere arbitrary.
[[nodiscard]] std::filesystem::path desktopUserDataPath(std::string_view org,
                                                        std::string_view app);

}  // namespace eng::client
