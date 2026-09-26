#include <editor/build/editor-deploy-manifest.h>
#include <array>
#include <charconv>
#include <nlohmann/json.hpp>

namespace eng::editor {

namespace {

  /// The string at @p key of @p root, or empty — never a throw, whatever
  /// the file holds there.
  std::string stringAt(const nlohmann::json& root, std::string_view key) {
    const auto found = root.find(key);
    return found != root.end() && found->is_string()
               ? found->get<std::string>()
               : std::string{};
  }

  /// Every string in the array at @p key of @p root.
  std::vector<std::string> stringsAt(const nlohmann::json& root,
                                     std::string_view key) {
    std::vector<std::string> out;
    const auto found = root.find(key);
    for (size_t i = 0; found != root.end() && found->is_array() &&
                       i < found->size();
         ++i) {
      if ((*found)[i].is_string()) {
        out.push_back((*found)[i].get<std::string>());
      }
    }
    return out;
  }

  /// @p hash in hex, as a JSON string, since a JSON number
  /// past 2^53 does not survive every reader.
  std::string hexHash(uint64_t hash) {
    std::array<char, 17> text{};
    (void)std::to_chars(text.data(), text.data() + 16, hash, 16);
    return text.data();
  }

  /// What `hexHash` wrote, or 0 when @p text is not that.
  uint64_t parseHexHash(std::string_view text) {
    uint64_t hash = 0;
    const auto [end, ec] =
        std::from_chars(text.data(), text.data() + text.size(), hash, 16);
    return ec == std::errc{} && end == text.data() + text.size() ? hash : 0;
  }

}  // namespace

std::string serializeDeployManifest(const EditorDeployManifest& manifest) {
  const nlohmann::json root = {{"schema", EDITOR_DEPLOY_SCHEMA},
                               {"name", manifest.name},
                               {"levels", manifest.levels},
                               {"start_level", manifest.start_level},
                               {"has_logic", manifest.has_logic},
                               {"logic_hash", hexHash(manifest.logic_hash)}};
  return root.dump(2) + "\n";
}

std::optional<EditorDeployManifest>
parseDeployManifest(std::string_view text) {
  const nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
  if (root.is_discarded() || !root.is_object() ||
      stringAt(root, "schema") != EDITOR_DEPLOY_SCHEMA) {
    return std::nullopt;
  }
  EditorDeployManifest manifest;
  manifest.name = stringAt(root, "name");
  manifest.start_level = stringAt(root, "start_level");
  const auto logic = root.find("has_logic");
  manifest.has_logic = logic != root.end() && logic->is_boolean() &&
                       logic->get<bool>();
  manifest.levels = stringsAt(root, "levels");
  manifest.logic_hash = parseHexHash(stringAt(root, "logic_hash"));
  return manifest;
}

}  // namespace eng::editor
