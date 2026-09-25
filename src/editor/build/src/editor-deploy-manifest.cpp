#include <editor/build/editor-deploy-manifest.h>
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

}  // namespace

std::string serializeDeployManifest(const EditorDeployManifest& manifest) {
  const nlohmann::json root = {{"schema", EDITOR_DEPLOY_SCHEMA},
                               {"name", manifest.name},
                               {"levels", manifest.levels},
                               {"start_level", manifest.start_level},
                               {"has_logic", manifest.has_logic}};
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
  return manifest;
}

}  // namespace eng::editor
