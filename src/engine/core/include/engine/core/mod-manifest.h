#pragma once

#include <string>
#include <vector>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// ModManifest: Parsed representation of a mod's manifest.json file.
//
// Contains identity, version, authorship, plugin path, and dependency list.
// Used by PluginManager to resolve load order and locate shared libraries.
//
// Thread Safety:
// - Value type; no thread-safety concerns.
// ============================================================================

struct CoreModManifest {
  /// Unique identifier for the mod (e.g. "com.example.mymod").
  std::string id;
  /// Human-readable display name of the mod.
  std::string name;
  /// Semantic version string (e.g. "1.2.3").
  std::string version;
  /// Author name or organization.
  std::string author;
  /// Short description of the mod's purpose.
  std::string description;
  /// Relative path to the plugin shared library within the mod directory.
  std::string plugin_path;
  /// List of mod IDs this mod depends on (loaded first).
  std::vector<std::string> dependencies;
};

}  // namespace eng
