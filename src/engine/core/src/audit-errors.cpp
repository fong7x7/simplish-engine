#include <engine/core/audit/audit-errors.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

namespace eng {

namespace {

  /// Map a severity string to AuditSeverity enum value.
  AuditSeverity parseSeverity(std::string_view s) {
    if (s == "error") {
      return AuditSeverity::ERROR;
    }
    if (s == "warn") {
      return AuditSeverity::WARN;
    }
    if (s == "info") {
      return AuditSeverity::INFO;
    }
    if (s == "debug") {
      return AuditSeverity::DEBUG;
    }
    return AuditSeverity::TRACE;
  }

  /// Read entire file contents into a string.
  std::optional<std::string> readWholeFile(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) {
      return std::nullopt;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
  }

  /// Extract the see_also array from a JSON error entry.
  std::vector<std::string> parseSeeAlso(const nlohmann::json& item) {
    std::vector<std::string> refs;
    if (item.contains("see_also") && item["see_also"].is_array()) {
      for (const auto& ref : item["see_also"]) {
        if (ref.is_string()) {
          refs.push_back(ref.get<std::string>());
        }
      }
    }
    return refs;
  }

  /// Parse a single error entry from JSON and insert into registry.
  void ingestErrorEntry(const nlohmann::json& item,
                        AuditErrorRegistry& registry) {
    if (!item.contains("code") || !item.contains("symbol")) {
      return;
    }
    AuditErrorEntry entry;
    entry.code = item.value("code", 0u);
    entry.domain = item.value("domain", "");
    entry.symbol = item.value("symbol", "");
    entry.severity = parseSeverity(item.value("severity", "trace"));
    entry.summary = item.value("summary", "");
    entry.detail = item.value("detail", "");
    entry.see_also = parseSeeAlso(item);
    auto [it, inserted] = registry.by_code.try_emplace(entry.code, entry);
    if (inserted) {
      registry.symbol_to_code[entry.symbol] = entry.code;
    }
  }

}  // namespace

namespace {

  /// Parse a JSON root object into an AuditErrorRegistry.
  AuditErrorRegistry parseErrorJson(const nlohmann::json& root) {
    AuditErrorRegistry registry;
    auto it_errors = root.find("errors");
    if (it_errors != root.end() && it_errors->is_array()) {
      for (const auto& item : *it_errors) {
        ingestErrorEntry(item, registry);
      }
    }
    return registry;
  }

}  // namespace

auto resolveErrorFilePath(std::string_view base_path)
    -> std::optional<std::filesystem::path> {
  const std::filesystem::path path(base_path);
  if (path.is_absolute() && !std::filesystem::exists(path)) {
    return std::nullopt;
  }
  return path / "error_codes.json";
}

std::optional<AuditErrorRegistry>
loadErrorRegistry(std::string_view base_path) {
  auto file_path = resolveErrorFilePath(base_path);
  if (!file_path) {
    return std::nullopt;
  }
  if (!std::filesystem::exists(*file_path)) {
    return AuditErrorRegistry{};
  }
  const auto contents = readWholeFile(*file_path);
  if (!contents) {
    return std::nullopt;
  }
  auto root = nlohmann::json::parse(*contents, nullptr, false);
  if (root.is_discarded() || !root.is_object()) {
    return std::nullopt;
  }
  return parseErrorJson(root);
}

const AuditErrorEntry* lookupError(const AuditErrorRegistry& registry,
                                   uint32_t code) {
  auto it = registry.by_code.find(code);
  if (it == registry.by_code.end()) {
    return nullptr;
  }
  return &it->second;
}

const AuditErrorEntry* lookupErrorBySymbol(const AuditErrorRegistry& registry,
                                           std::string_view symbol) {
  auto it = registry.symbol_to_code.find(std::string(symbol));
  if (it == registry.symbol_to_code.end()) {
    return nullptr;
  }
  return lookupError(registry, it->second);
}

bool registerModError(AuditErrorRegistry& registry,
                      const AuditErrorEntry& entry) {
  auto [it, inserted] = registry.by_code.try_emplace(entry.code, entry);
  if (!inserted) {
    return false;
  }
  registry.symbol_to_code[entry.symbol] = entry.code;
  return true;
}

}  // namespace eng
