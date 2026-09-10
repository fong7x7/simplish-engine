#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <editor/shell/editor-entity-id.h>
#include <editor/shell/editor-light-ops.h>
#include <editor/shell/editor-shape-kind.h>
#include <unordered_map>
#include <unordered_set>

namespace eng::editor {

namespace {

  /// Enough for "_999" and more; a project with a thousand crates on one
  /// tile has a problem this number is not part of.
  constexpr size_t SUFFIX_TEXT_CAPACITY = 16;
  /// The first number a minted id takes, and the width it is written to.
  constexpr int FIRST_ORDINAL = 1;
  /// How many digits an ordinal is padded to, so `crate_02` sorts beside
  /// `crate_10` in a file anyone reads.
  constexpr int ORDINAL_DIGITS = 2;

  bool isIdentifierChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
  }

  char lowered(char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }

  /// Whether an id may not begin with @p c, which a digit may not.
  bool badFirstChar(char c) {
    return c >= '0' && c <= '9';
  }

  /// `<base>_<n>`, zero-padded.
  std::string numbered(std::string_view base, int ordinal) {
    std::array<char, SUFFIX_TEXT_CAPACITY> suffix{};
    const int written = std::snprintf(suffix.data(), suffix.size(), "_%0*d",
                                      ORDINAL_DIGITS, ordinal);
    if (written <= 0) {
      return std::string(base);
    }
    const size_t length =
        std::min(static_cast<size_t>(written), suffix.size() - 1);
    return std::string(base).append(suffix.data(), length);
  }

  /// The first of `base`, `base_2`, `base_3`, … that @p taken does not
  /// hold.
  std::string firstFree(std::string_view base,
                        const std::unordered_set<std::string>& taken) {
    std::string candidate(base);
    int suffix = 1;
    while (taken.contains(candidate)) {
      ++suffix;
      candidate = std::string(base).append("_").append(std::to_string(suffix));
    }
    return candidate;
  }

  /// The id an asset takes before uniqueness is enforced.
  std::string assetIdBase(const EditorAsset& asset) {
    if (asset.shape.has_value()) {
      return makeEditorIdentifier(editorShapeName(*asset.shape));
    }
    return editorAssetIdFromPath(asset.relative_path);
  }

  /// The first `<base>_NN` no entry of @p taken already uses.
  std::string firstFreeOrdinal(std::string_view base,
                               const std::unordered_set<std::string>& taken) {
    int ordinal = FIRST_ORDINAL;
    std::string candidate = numbered(base, ordinal);
    while (taken.contains(candidate)) {
      ++ordinal;
      candidate = numbered(base, ordinal);
    }
    return candidate;
  }

  /// Where each asset sits, by its id.
  std::unordered_map<std::string_view, size_t>
  assetsById(const std::vector<EditorAsset>& assets) {
    std::unordered_map<std::string_view, size_t> by_id;
    by_id.reserve(assets.size());
    for (size_t i = 0; i < assets.size(); ++i) {
      by_id.emplace(assets[i].id, i);
    }
    return by_id;
  }

  /// Point @p placement at its asset's new index. False when the asset is
  /// not there any more, which is the caller's cue to drop it.
  bool rebindOne(EditorPlacement& placement,
                 const std::vector<std::string>& previous_ids,
                 const std::unordered_map<std::string_view, size_t>& by_id) {
    if (placement.asset >= previous_ids.size()) {
      return false;
    }
    const auto found = by_id.find(previous_ids[placement.asset]);
    if (found == by_id.end()) {
      return false;
    }
    placement.asset = found->second;
    return true;
  }

  /// Every id the document's placements hold.
  std::unordered_set<std::string> placementIds(const EditorDocument& document) {
    std::unordered_set<std::string> taken;
    for (const EditorPlacement& placement : document.placements) {
      taken.insert(placement.id);
    }
    return taken;
  }

  /// Every id the document's player starts hold.
  std::unordered_set<std::string>
  playerStartIds(const EditorDocument& document) {
    std::unordered_set<std::string> taken;
    for (const EditorPlayerStart& start : document.player_starts) {
      taken.insert(start.id);
    }
    return taken;
  }

  /// Every id the document's lights hold.
  std::unordered_set<std::string> lightIds(const EditorDocument& document) {
    std::unordered_set<std::string> taken;
    for (const EditorLight& light : document.lights) {
      taken.insert(light.id);
    }
    return taken;
  }

}  // namespace

std::string makeEditorIdentifier(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) {
    const char lower = lowered(c);
    if (isIdentifierChar(lower)) {
      out.push_back(lower);
    } else if (!out.empty() && out.back() != '_') {
      // One underscore per run of anything unusable, and never a leading
      // one — the trailing one is trimmed below.
      out.push_back('_');
    }
  }
  while (!out.empty() && out.back() == '_') {
    out.pop_back();
  }
  if (out.empty()) {
    return std::string(EDITOR_ID_FALLBACK);
  }
  if (badFirstChar(out.front())) {
    out.insert(out.begin(), '_');
  }
  return out;
}

std::string editorQualifiedId(EditorIdKind kind, std::string_view id) {
  return std::string(editorIdKindPrefix(kind)).append(":").append(id);
}

std::string editorAssetIdFromPath(const std::filesystem::path& relative_path) {
  std::filesystem::path without_extension = relative_path;
  without_extension.replace_extension();
  return makeEditorIdentifier(without_extension.generic_string());
}

void assignEditorAssetIds(std::vector<EditorAsset>& assets) {
  std::unordered_set<std::string> taken;
  for (EditorAsset& asset : assets) {
    asset.id = firstFree(assetIdBase(asset), taken);
    taken.insert(asset.id);
  }
}

std::string editorAssetRef(const EditorAsset& asset) {
  return editorQualifiedId(asset.shape.has_value() ? EditorIdKind::SHAPE
                                                   : EditorIdKind::MESH,
                           asset.id);
}

std::string editorPlacementRef(const EditorPlacement& placement) {
  return editorQualifiedId(EditorIdKind::PROP, placement.id);
}

std::string editorLightRef(const EditorLight& light) {
  return editorQualifiedId(EditorIdKind::LIGHT, light.id);
}

std::string editorPlayerStartRef(const EditorPlayerStart& start) {
  return editorQualifiedId(EditorIdKind::PLAYER_START, start.id);
}

size_t rebindPlacementAssets(EditorDocument& document,
                             const std::vector<std::string>& previous_ids,
                             const std::vector<EditorAsset>& assets) {
  const std::unordered_map<std::string_view, size_t> by_id = assetsById(assets);
  std::vector<EditorPlacement> kept;
  kept.reserve(document.placements.size());
  for (EditorPlacement& placement : document.placements) {
    if (rebindOne(placement, previous_ids, by_id)) {
      kept.push_back(std::move(placement));
    }
  }
  const size_t dropped = document.placements.size() - kept.size();
  document.placements = std::move(kept);
  return dropped;
}

std::string mintEditorPlacementId(const EditorDocument& document,
                                  const EditorAsset& asset) {
  // An asset with no id of its own — one the list no longer holds — still
  // gets a placement that can be named, just not for what it is.
  const std::string base =
      asset.id.empty() ? std::string(editorIdKindPrefix(EditorIdKind::PROP))
                       : asset.id;
  return firstFreeOrdinal(base, placementIds(document));
}

std::string mintEditorLightId(const EditorDocument& document,
                              EditorLightKind kind) {
  return firstFreeOrdinal(makeEditorIdentifier(editorLightKindId(kind)),
                          lightIds(document));
}

std::string mintEditorPlayerStartId(const EditorDocument& document) {
  return firstFreeOrdinal("start", playerStartIds(document));
}

}  // namespace eng::editor
