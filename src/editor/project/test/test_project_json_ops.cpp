#include <catch2/catch_test_macros.hpp>
#include <editor/project/project-json-ops.h>

using eng::editor::parseProjectMetadata;
using eng::editor::parseRecentProjects;
using eng::editor::ProjectMetadata;
using eng::editor::ProjectProjection;
using eng::editor::RecentProjectEntry;
using eng::editor::RecentProjectsList;
using eng::editor::serializeProjectMetadata;
using eng::editor::serializeRecentProjects;

TEST_CASE("parseProjectMetadata reads every field") {
  const auto* json = R"({
    "name": "Transit Station",
    "engine_version": "0.1.0",
    "created_at": "2026-08-01T09:00:00Z",
    "last_opened_at": "2026-08-22T10:30:00Z",
    "default_workspace": "Encounter"
  })";

  auto meta = parseProjectMetadata(json);
  REQUIRE(meta.has_value());
  REQUIRE(meta->name == "Transit Station");
  REQUIRE(meta->engine_version == "0.1.0");
  REQUIRE(meta->created_at == "2026-08-01T09:00:00Z");
  REQUIRE(meta->last_opened_at == "2026-08-22T10:30:00Z");
  REQUIRE(meta->default_workspace == "Encounter");
}

TEST_CASE("parseProjectMetadata defaults the workspace when absent") {
  auto meta = parseProjectMetadata(R"({"name": "Untitled"})");
  REQUIRE(meta.has_value());
  REQUIRE(meta->name == "Untitled");
  REQUIRE(meta->default_workspace == "Level");
}

TEST_CASE("parseProjectMetadata rejects malformed JSON without throwing") {
  REQUIRE_FALSE(parseProjectMetadata("{not json").has_value());
  REQUIRE_FALSE(parseProjectMetadata("").has_value());
}

TEST_CASE("parseProjectMetadata rejects a non-object document") {
  REQUIRE_FALSE(parseProjectMetadata("[1, 2, 3]").has_value());
  REQUIRE_FALSE(parseProjectMetadata("\"a string\"").has_value());
}

TEST_CASE("project metadata survives a serialize/parse round trip") {
  ProjectMetadata original;
  original.name = "Flooded Retail";
  original.engine_version = "0.1.0";
  original.created_at = "2026-08-02T12:00:00Z";
  original.last_opened_at = "2026-08-22T08:00:00Z";
  original.default_workspace = "Level";
  original.projection = ProjectProjection::ISOMETRIC;

  auto restored = parseProjectMetadata(serializeProjectMetadata(original));
  REQUIRE(restored.has_value());
  REQUIRE(restored->name == original.name);
  REQUIRE(restored->engine_version == original.engine_version);
  REQUIRE(restored->created_at == original.created_at);
  REQUIRE(restored->last_opened_at == original.last_opened_at);
  REQUIRE(restored->default_workspace == original.default_workspace);
  REQUIRE(restored->projection == original.projection);
}

TEST_CASE("a project written before projections was dimetric") {
  // Every project that exists today has no projection field, and every one
  // of them was authored against the dimetric view. Reading them as
  // anything else would move their level under them.
  auto meta = parseProjectMetadata(R"({"name": "Untitled"})");
  REQUIRE(meta.has_value());
  REQUIRE(meta->projection == ProjectProjection::DIMETRIC);
}

TEST_CASE("an unrecognised projection name reads as dimetric") {
  // A hand-edited or newer file should open at the default rather than
  // refuse to load: the setting is recoverable from the View menu.
  auto meta = parseProjectMetadata(R"({"projection": "trimetric"})");
  REQUIRE(meta.has_value());
  REQUIRE(meta->projection == ProjectProjection::DIMETRIC);
}

TEST_CASE("the projection is written by name") {
  ProjectMetadata meta;
  meta.projection = ProjectProjection::ISOMETRIC;
  // A word, not an ordinal: the file is meant to be read and hand-edited,
  // and an ordinal would silently change meaning if the enum ever grew.
  REQUIRE(serializeProjectMetadata(meta).find("\"isometric\"") !=
          std::string::npos);
}

TEST_CASE("parseRecentProjects returns an empty list when entries are absent") {
  auto list = parseRecentProjects("{}");
  REQUIRE(list.has_value());
  REQUIRE(list->entries.empty());
}

TEST_CASE("parseRecentProjects drops rows that carry no path") {
  const auto* json = R"({"entries": [
    {"path": "/a", "name": "A"},
    {"name": "no path"},
    {"path": "/b", "name": "B"}
  ]})";

  auto list = parseRecentProjects(json);
  REQUIRE(list.has_value());
  REQUIRE(list->entries.size() == 2);
  REQUIRE(list->entries[0].path == "/a");
  REQUIRE(list->entries[1].path == "/b");
}

TEST_CASE("recent projects survive a serialize/parse round trip") {
  RecentProjectsList original;
  original.entries.push_back(
      RecentProjectEntry{"/projects/one", "One", "2026-08-22T10:00:00Z"});
  original.entries.push_back(
      RecentProjectEntry{"/projects/two", "Two", "2026-08-21T10:00:00Z"});

  auto restored = parseRecentProjects(serializeRecentProjects(original));
  REQUIRE(restored.has_value());
  REQUIRE(restored->entries.size() == 2);
  REQUIRE(restored->entries[0].path == "/projects/one");
  REQUIRE(restored->entries[0].name == "One");
  REQUIRE(restored->entries[1].last_opened_at == "2026-08-21T10:00:00Z");
}
