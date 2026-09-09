#include <editor/project/project-text-file.h>
#include <fstream>
#include <ios>
#include <sstream>
#include <system_error>

namespace eng::editor {

std::optional<std::string>
readProjectTextFile(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return std::nullopt;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  if (in.bad()) {
    return std::nullopt;
  }
  return buffer.str();
}

bool writeProjectTextFile(const std::filesystem::path& path,
                          std::string_view contents) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    return false;
  }
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }
  out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
  out.flush();
  return out.good();
}

}  // namespace eng::editor
