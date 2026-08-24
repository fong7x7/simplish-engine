#pragma once

#include <cstdint>
#include <string>

namespace eng {

/// Information about a subscribed Workshop item.
struct SteamWorkshopItemInfo {
  /// Workshop published file ID.
  uint64_t item_id = 0;
  /// Local filesystem path where the item is installed.
  std::string install_path{};
  /// Size of the installed item in bytes.
  uint64_t size_on_disk = 0;
  /// True if the item has been downloaded and is ready to load.
  bool is_installed = false;
};

}  // namespace eng
