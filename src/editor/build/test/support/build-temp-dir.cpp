#include "build-temp-dir.h"
#include <cstdint>
#include <system_error>

namespace eng::editor::test {

BuildTempDir::BuildTempDir(const std::string& label)
  : path_(std::filesystem::temp_directory_path() /
          ("simplish-build-test-" + label + "-" +
           // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
           std::to_string(reinterpret_cast<uintptr_t>(this)))) {
  std::error_code ec;
  std::filesystem::remove_all(path_, ec);
  std::filesystem::create_directories(path_, ec);
}

BuildTempDir::~BuildTempDir() {
  std::error_code ec;
  std::filesystem::remove_all(path_, ec);
}

}  // namespace eng::editor::test
