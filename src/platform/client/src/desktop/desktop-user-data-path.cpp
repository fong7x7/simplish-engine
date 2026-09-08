#include <engine/client/desktop-user-data-path.h>
#include <string>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#include <SDL3/SDL.h>
#pragma clang diagnostic pop

namespace eng::client {

std::filesystem::path desktopUserDataPath(std::string_view org,
                                          std::string_view app) {
  // Copied into strings because SDL takes C strings, and a string_view
  // carries no promise of a terminator.
  const std::string org_name(org);
  const std::string app_name(app);
  char* prefs = SDL_GetPrefPath(org_name.c_str(), app_name.c_str());
  if (prefs == nullptr) {
    return {};
  }
  std::filesystem::path path(prefs);
  SDL_free(prefs);
  return path;
}

}  // namespace eng::client
