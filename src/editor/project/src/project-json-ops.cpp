#include <editor/project/project-json-ops.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>

namespace eng::editor {

namespace {

  /// Parse without throwing — this build has exceptions disabled
  /// (docs/decisions/ADR-001-no-exceptions.md). `parse` with
  /// `allow_exceptions = false` reports failure via `is_discarded()`.
  std::optional<nlohmann::json> tryParse(std::string_view json) {
    nlohmann::json parsed =
        nlohmann::json::parse(std::string(json), nullptr, false);
    if (parsed.is_discarded()) {
      return std::nullopt;
    }
    return parsed;
  }

  /// Read the two settings that decide how the world is drawn.
  void parseLook(const nlohmann::json& parsed, ProjectMetadata& meta) {
    // Both are absent in every project written before the setting existed,
    // and those were all authored against the dimetric projection and
    // drawn smooth.
    meta.projection = projectProjectionFromName(
        parsed.value("projection", std::string("dimetric")));
    meta.shading =
        projectShadingFromName(parsed.value("shading", std::string("smooth")));
  }

  /// One recent-projects row, or nullopt for a row that is not one.
  std::optional<RecentProjectEntry>
  parseRecentEntry(const nlohmann::json& item) {
    if (!item.is_object()) {
      return std::nullopt;
    }
    RecentProjectEntry entry;
    entry.path = item.value("path", "");
    // A row without a path cannot be reopened, so it is dropped rather than
    // surfaced as an unusable launcher entry.
    if (entry.path.empty()) {
      return std::nullopt;
    }
    entry.name = item.value("name", "");
    entry.last_opened_at = item.value("last_opened_at", "");
    return entry;
  }

}  // namespace

std::optional<ProjectMetadata> parseProjectMetadata(std::string_view json) {
  auto parsed = tryParse(json);
  if (!parsed || !parsed->is_object()) {
    return std::nullopt;
  }
  ProjectMetadata meta;
  meta.name = parsed->value("name", "");
  meta.engine_version = parsed->value("engine_version", "");
  meta.created_at = parsed->value("created_at", "");
  meta.last_opened_at = parsed->value("last_opened_at", "");
  meta.default_workspace = parsed->value("default_workspace", "Level");
  parseLook(*parsed, meta);
  return meta;
}

std::string serializeProjectMetadata(const ProjectMetadata& meta) {
  nlohmann::json out;
  out["name"] = meta.name;
  out["engine_version"] = meta.engine_version;
  out["created_at"] = meta.created_at;
  out["last_opened_at"] = meta.last_opened_at;
  out["default_workspace"] = meta.default_workspace;
  out["projection"] = projectProjectionName(meta.projection);
  out["shading"] = projectShadingName(meta.shading);
  return out.dump(2);
}

std::optional<RecentProjectsList> parseRecentProjects(std::string_view json) {
  auto parsed = tryParse(json);
  if (!parsed || !parsed->is_object()) {
    return std::nullopt;
  }
  RecentProjectsList list;
  auto entries = parsed->find("entries");
  if (entries == parsed->end() || !entries->is_array()) {
    return list;
  }
  for (const auto& item : *entries) {
    if (auto entry = parseRecentEntry(item)) {
      list.entries.push_back(std::move(*entry));
    }
  }
  return list;
}

std::string serializeRecentProjects(const RecentProjectsList& list) {
  nlohmann::json out;
  auto entries = nlohmann::json::array();
  for (const auto& entry : list.entries) {
    nlohmann::json row;
    row["path"] = entry.path;
    row["name"] = entry.name;
    row["last_opened_at"] = entry.last_opened_at;
    entries.push_back(std::move(row));
  }
  out["entries"] = std::move(entries);
  return out.dump(2);
}

}  // namespace eng::editor
