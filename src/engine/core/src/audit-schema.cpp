#include <engine/core/audit/audit-schema.h>
#include <engine/core/fnv1a.h>
#include <filesystem>

namespace eng {

namespace {

  /// Combine category and event_type into a single map key.
  uint32_t schemaKey(uint16_t category, uint16_t event_type) {
    return (static_cast<uint32_t>(category) << 16) | event_type;
  }

}  // namespace

std::optional<AuditSchemaRegistry>
loadSchemaRegistry(std::string_view base_path) {
  const std::filesystem::path path(base_path);
  if (path.is_absolute() && !std::filesystem::exists(path)) {
    return std::nullopt;
  }
  return AuditSchemaRegistry{};
}

const AuditSchemaEntry* lookupSchema(const AuditSchemaRegistry& registry,
                                     uint16_t category, uint16_t event_type) {
  const auto key = schemaKey(category, event_type);
  auto it = registry.entries.find(key);
  if (it == registry.entries.end()) {
    return nullptr;
  }
  return &it->second;
}

bool registerModSchema(AuditSchemaRegistry& registry,
                       const AuditSchemaEntry& entry) {
  const auto key = schemaKey(entry.category, entry.event_type);
  auto [it, inserted] = registry.entries.try_emplace(key, entry);
  return inserted;
}

uint64_t computeSchemaVersionHash(const AuditSchemaRegistry& registry) {
  uint64_t hash = FNV1A_OFFSET;
  for (const auto& [key, entry] : registry.entries) {
    hash ^= key;
    hash *= FNV1A_PRIME;
  }
  return hash;
}

}  // namespace eng
