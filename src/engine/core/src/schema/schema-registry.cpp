#include <algorithm>
#include <engine/core/logger.h>
#include <engine/core/schema/schema-field-type.h>
#include <engine/core/schema/schema-registry.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

namespace eng::schema {

namespace {

  constexpr auto LOG_TAG = "schema";
  constexpr std::string_view SCHEMA_SUFFIX = ".schema";

  // NOLINTNEXTLINE(cert-err58-cpp,bugprone-throwing-static-initialization)
  const std::unordered_map<std::string, SchemaFieldType> FIELD_TYPE_MAP = {
      {"string", SchemaFieldType::STRING},
      {"integer", SchemaFieldType::INTEGER},
      {"number", SchemaFieldType::NUMBER},
      {"boolean", SchemaFieldType::BOOLEAN},
      {"object", SchemaFieldType::OBJECT},
      {"array", SchemaFieldType::ARRAY},
  };

  SchemaFieldType parseFieldType(const std::string& type_str) {
    auto it = FIELD_TYPE_MAP.find(type_str);
    return (it != FIELD_TYPE_MAP.end()) ? it->second : SchemaFieldType::STRING;
  }

  void parseRuleEnums(SchemaFieldRule& rule, const nlohmann::json& prop) {
    if (!prop.contains("enum") || !prop["enum"].is_array()) {
      return;
    }
    for (const auto& val : prop["enum"]) {
      if (val.is_string()) {
        rule.enum_values.push_back(val.get<std::string>());
      }
    }
  }

  void parseRuleRanges(SchemaFieldRule& rule, const nlohmann::json& prop) {
    if (prop.contains("minimum") && prop["minimum"].is_number()) {
      rule.minimum = prop["minimum"].get<double>();
    }
    if (prop.contains("maximum") && prop["maximum"].is_number()) {
      rule.maximum = prop["maximum"].get<double>();
    }
    if (prop.contains("exclusiveMinimum") &&
        prop["exclusiveMinimum"].is_number()) {
      rule.exclusive_min = prop["exclusiveMinimum"].get<double>();
    }
    if (prop.contains("exclusiveMaximum") &&
        prop["exclusiveMaximum"].is_number()) {
      rule.exclusive_max = prop["exclusiveMaximum"].get<double>();
    }
  }

  void parseRuleStringAndArray(SchemaFieldRule& rule,
                               const nlohmann::json& prop) {
    if (prop.contains("pattern") && prop["pattern"].is_string()) {
      rule.pattern = prop["pattern"].get<std::string>();
    }
    if (prop.contains("minItems") && prop["minItems"].is_number_integer()) {
      rule.min_items = prop["minItems"].get<uint32_t>();
    }
    if (prop.contains("maxItems") && prop["maxItems"].is_number_integer()) {
      rule.max_items = prop["maxItems"].get<uint32_t>();
    }
  }

  SchemaFieldRule parseFieldRule(const std::string& name,
                                 const nlohmann::json& prop) {
    SchemaFieldRule rule{};
    rule.field_name = name;
    if (prop.contains("type") && prop["type"].is_string()) {
      rule.type = parseFieldType(prop["type"].get<std::string>());
    }
    parseRuleEnums(rule, prop);
    parseRuleRanges(rule, prop);
    parseRuleStringAndArray(rule, prop);
    return rule;
  }

  std::vector<std::string> extractRequired(const nlohmann::json& node) {
    std::vector<std::string> result{};
    if (!node.contains("required") || !node["required"].is_array()) {
      return result;
    }
    for (const auto& r : node["required"]) {
      if (r.is_string()) {
        result.push_back(r.get<std::string>());
      }
    }
    return result;
  }

  bool isObjectProperty(const nlohmann::json& prop) {
    return prop.is_object() && prop.contains("type") &&
           prop["type"].is_string() &&
           prop["type"].get<std::string>() == "object";
  }

  /// Parsing context groups parameters for recursive property parsing.
  struct ParseCtx {
    /// Output: top-level field rules.
    std::vector<SchemaFieldRule>& fields;
    /// Output: nested object field rules keyed by path prefix.
    std::unordered_map<std::string, std::vector<SchemaFieldRule>>& nested;
    /// Required field names for the current scope.
    const std::vector<std::string>& required_fields;
    /// JSON pointer prefix for nested path (empty at root).
    std::string prefix;
  };

  void parseProperties(const nlohmann::json& schema_json, ParseCtx& ctx);

  // Recursion mirrors the nested object structure of JSON schemas.
  // NOLINTNEXTLINE(misc-no-recursion)
  void parseNestedObject(const std::string& name, const nlohmann::json& prop,
                         ParseCtx& ctx) {
    auto nested_prefix = ctx.prefix.empty() ? name : ctx.prefix + "/" + name;
    auto nested_required = extractRequired(prop);
    ParseCtx child{ctx.nested[nested_prefix], ctx.nested, nested_required,
                   nested_prefix};
    parseProperties(prop, child);
  }

  void parseLeafField(const std::string& name, const nlohmann::json& prop,
                      ParseCtx& ctx) {
    auto rule = parseFieldRule(name, prop);
    auto it = std::ranges::find(ctx.required_fields, name);
    if (it != ctx.required_fields.end()) {
      rule.requirement = FieldRequirement::REQUIRED;
    }
    auto& target = ctx.prefix.empty() ? ctx.fields : ctx.nested[ctx.prefix];
    target.push_back(std::move(rule));
  }

  // Recursion mirrors the nested object structure of JSON schemas.
  // NOLINTNEXTLINE(misc-no-recursion)
  void parseProperties(const nlohmann::json& schema_json, ParseCtx& ctx) {
    if (!schema_json.contains("properties")) {
      return;
    }
    const auto& props = schema_json["properties"];
    if (!props.is_object()) {
      return;
    }

    for (const auto& [name, prop] : props.items()) {
      if (isObjectProperty(prop)) {
        parseNestedObject(name, prop, ctx);
      } else {
        parseLeafField(name, prop, ctx);
      }
    }
  }

  std::optional<nlohmann::json>
  readSchemaJson(const std::filesystem::path& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
      return std::nullopt;
    }
    auto doc = nlohmann::json::parse(file, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
      Logger::warn(LOG_TAG, "Malformed schema: " + file_path.string());
      return std::nullopt;
    }
    return doc;
  }

  std::optional<SchemaDefinition>
  parseSchemaHeader(const nlohmann::json& doc,
                    const std::filesystem::path& file_path) {
    SchemaDefinition def{};
    def.schema_id = doc.value("$id", "");
    def.title = doc.value("title", "");
    if (def.schema_id.empty()) {
      Logger::warn(LOG_TAG, "Schema missing $id: " + file_path.string());
      return std::nullopt;
    }
    return def;
  }

  std::optional<SchemaDefinition>
  parseSchemaFile(const std::filesystem::path& file_path) {
    auto doc = readSchemaJson(file_path);
    if (!doc) {
      return std::nullopt;
    }
    auto def = parseSchemaHeader(*doc, file_path);
    if (!def) {
      return std::nullopt;
    }
    auto required_fields = extractRequired(*doc);
    ParseCtx ctx{def->fields, def->nested_objects, required_fields, ""};
    parseProperties(*doc, ctx);
    return def;
  }

  bool isSchemaFile(const std::filesystem::path& path) {
    if (path.extension() != ".json") {
      return false;
    }
    auto stem = path.stem().string();
    return stem.ends_with(SCHEMA_SUFFIX);
  }

  void loadSchemaEntry(SchemaRegistry& registry,
                       const std::filesystem::path& path) {
    auto def = parseSchemaFile(path);
    if (!def) {
      return;
    }
    auto id = def->schema_id;
    if (registry.schemas.contains(id)) {
      Logger::warn(LOG_TAG, "Duplicate schema_id '" + id + "', overwriting");
    }
    registry.schemas[id] = std::move(*def);
  }

}  // namespace

std::optional<SchemaRegistry>
SchemaRegistry::loadFromDir(const SchemaRegistryContext& ctx) {
  std::filesystem::path dir(ctx.schemas_dir);
  std::error_code ec{};
  if (!std::filesystem::is_directory(dir, ec)) {
    Logger::error(LOG_TAG, "Schema directory not found: " + dir.string());
    return std::nullopt;
  }

  SchemaRegistry registry{};
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (entry.is_regular_file() && isSchemaFile(entry.path())) {
      loadSchemaEntry(registry, entry.path());
    }
  }

  Logger::info(LOG_TAG, "Loaded " + std::to_string(registry.schemas.size()) +
                            " schema(s)");
  return registry;
}

const SchemaDefinition*
SchemaRegistry::findSchema(const SchemaRegistry& registry,
                           std::string_view schema_id) {
  auto it = registry.schemas.find(std::string(schema_id));
  if (it == registry.schemas.end()) {
    return nullptr;
  }
  return &it->second;
}

}  // namespace eng::schema
