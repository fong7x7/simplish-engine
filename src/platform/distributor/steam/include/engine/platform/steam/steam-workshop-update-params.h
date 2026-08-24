#pragma once

#include <span>
#include <string_view>

namespace eng {

/// Parameters for submitting a Workshop item update.
struct SteamWorkshopUpdateParams {
  /// Path to the folder containing the item content to upload.
  std::string_view content_folder;
  /// Display title for the Workshop item.
  std::string_view title;
  /// Description text shown on the Workshop page.
  std::string_view description;
  /// Path to the preview image file for the Workshop listing.
  std::string_view preview_image_path;
  /// Change note describing what was updated in this version.
  std::string_view change_note;
  /// Tags to apply to the Workshop item for filtering.
  std::span<const std::string_view> tags;
};

}  // namespace eng
