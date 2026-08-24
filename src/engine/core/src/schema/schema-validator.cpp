#include <engine/core/schema/schema-field-type.h>
#include <engine/core/schema/schema-validator.h>
#include <nlohmann/json.hpp>
#include <regex>
#include <string>

namespace eng::schema {

namespace {

  bool checkJsonType(const nlohmann::json& val, SchemaFieldType expected) {
    switch (expected) {
      case SchemaFieldType::STRING:
        return val.is_string();
      case SchemaFieldType::INTEGER:
        return val.is_number_integer();
      case SchemaFieldType::NUMBER:
        return val.is_number();
      case SchemaFieldType::BOOLEAN:
        return val.is_boolean();
      case SchemaFieldType::OBJECT:
        return val.is_object();
      case SchemaFieldType::ARRAY:
        return val.is_array();
    }
    return false;
  }

  std::string typeToString(SchemaFieldType type) {
    switch (type) {
      case SchemaFieldType::STRING:
        return "string";
      case SchemaFieldType::INTEGER:
        return "integer";
      case SchemaFieldType::NUMBER:
        return "number";
      case SchemaFieldType::BOOLEAN:
        return "boolean";
      case SchemaFieldType::OBJECT:
        return "object";
      case SchemaFieldType::ARRAY:
        return "array";
    }
    return "unknown";
  }

  void validateEnumValues(const nlohmann::json& val,
                          const SchemaFieldRule& rule,
                          const std::string& field_path,
                          ValidationResult& result) {
    if (rule.enum_values.empty() || !val.is_string()) {
      return;
    }
    auto str = val.get<std::string>();
    for (const auto& allowed : rule.enum_values) {
      if (str == allowed) {
        return;
      }
    }
    ValidationResult::addError(result, field_path,
                               ValidationErrorKind::ENUM_VALUE_UNKNOWN,
                               "Value '" + str + "' is not a valid enum value");
  }

  void validateInclusiveRange(const nlohmann::json& val,
                              const SchemaFieldRule& rule,
                              const std::string& field_path,
                              ValidationResult& result) {
    auto num = val.get<double>();
    if (rule.minimum && num < *rule.minimum) {
      ValidationResult::addError(
          result, field_path, ValidationErrorKind::OUT_OF_RANGE,
          "Value " + std::to_string(num) + " below minimum " +
              std::to_string(*rule.minimum));
    }
    if (rule.maximum && num > *rule.maximum) {
      ValidationResult::addError(
          result, field_path, ValidationErrorKind::OUT_OF_RANGE,
          "Value " + std::to_string(num) + " above maximum " +
              std::to_string(*rule.maximum));
    }
  }

  void validateExclusiveRange(const nlohmann::json& val,
                              const SchemaFieldRule& rule,
                              const std::string& field_path,
                              ValidationResult& result) {
    auto num = val.get<double>();
    if (rule.exclusive_min && num <= *rule.exclusive_min) {
      ValidationResult::addError(
          result, field_path, ValidationErrorKind::OUT_OF_RANGE,
          "Value must be > " + std::to_string(*rule.exclusive_min));
    }
    if (rule.exclusive_max && num >= *rule.exclusive_max) {
      ValidationResult::addError(
          result, field_path, ValidationErrorKind::OUT_OF_RANGE,
          "Value must be < " + std::to_string(*rule.exclusive_max));
    }
  }

  void validateNumericRange(const nlohmann::json& val,
                            const SchemaFieldRule& rule,
                            const std::string& field_path,
                            ValidationResult& result) {
    if (!val.is_number()) {
      return;
    }
    validateInclusiveRange(val, rule, field_path, result);
    validateExclusiveRange(val, rule, field_path, result);
  }

  void validatePattern(const nlohmann::json& val, const SchemaFieldRule& rule,
                       const std::string& field_path,
                       ValidationResult& result) {
    if (!rule.pattern || !val.is_string()) {
      return;
    }
    auto str = val.get<std::string>();
    if (!std::regex_match(str, std::regex(*rule.pattern))) {
      ValidationResult::addError(
          result, field_path, ValidationErrorKind::CUSTOM,
          "Value '" + str + "' does not match pattern '" + *rule.pattern + "'");
    }
  }

  void validateArrayLength(const nlohmann::json& val,
                           const SchemaFieldRule& rule,
                           const std::string& field_path,
                           ValidationResult& result) {
    if (!val.is_array()) {
      return;
    }
    auto size = static_cast<uint32_t>(val.size());
    if (rule.min_items && size < *rule.min_items) {
      ValidationResult::addError(
          result, field_path, ValidationErrorKind::OUT_OF_RANGE,
          "Array has " + std::to_string(size) + " items, minimum " +
              std::to_string(*rule.min_items));
    }
    if (rule.max_items && size > *rule.max_items) {
      ValidationResult::addError(
          result, field_path, ValidationErrorKind::OUT_OF_RANGE,
          "Array has " + std::to_string(size) + " items, maximum " +
              std::to_string(*rule.max_items));
    }
  }

  void validateFieldValue(const nlohmann::json& val,
                          const SchemaFieldRule& rule,
                          const std::string& field_path,
                          ValidationResult& result) {
    validateEnumValues(val, rule, field_path, result);
    validateNumericRange(val, rule, field_path, result);
    validatePattern(val, rule, field_path, result);
    validateArrayLength(val, rule, field_path, result);
  }

  void reportMissingField(const SchemaFieldRule& rule,
                          const std::string& field_path,
                          ValidationResult& result) {
    if (rule.requirement == FieldRequirement::REQUIRED) {
      ValidationResult::addError(
          result, field_path, ValidationErrorKind::MISSING_REQUIRED_FIELD,
          "Required field '" + rule.field_name + "' is missing");
    }
  }

  void validateField(const nlohmann::json& parent, const SchemaFieldRule& rule,
                     const std::string& path_prefix, ValidationResult& result) {
    auto field_path = path_prefix + "/" + rule.field_name;
    if (!parent.contains(rule.field_name)) {
      reportMissingField(rule, field_path, result);
      return;
    }
    const auto& val = parent[rule.field_name];
    if (!checkJsonType(val, rule.type)) {
      ValidationResult::addError(
          result, field_path, ValidationErrorKind::WRONG_TYPE,
          "Expected type '" + typeToString(rule.type) + "'");
      return;
    }
    validateFieldValue(val, rule, field_path, result);
  }

  void validateFields(const nlohmann::json& json,
                      const std::vector<SchemaFieldRule>& rules,
                      const std::string& path_prefix,
                      ValidationResult& result) {
    for (const auto& rule : rules) {
      validateField(json, rule, path_prefix, result);
    }
  }

  const nlohmann::json* resolveNestedPath(const nlohmann::json& json,
                                          const std::string& prefix,
                                          std::string& built_path) {
    const nlohmann::json* current = &json;
    auto remaining = prefix;
    while (!remaining.empty()) {
      auto slash = remaining.find('/');
      auto segment =
          (slash == std::string::npos) ? remaining : remaining.substr(0, slash);
      if (!current->contains(segment) || !(*current)[segment].is_object()) {
        return nullptr;
      }
      current = &(*current)[segment];
      built_path += "/" + std::string(segment);
      remaining =
          (slash == std::string::npos) ? "" : remaining.substr(slash + 1);
    }
    return current;
  }

  void validateNestedObjects(
      const nlohmann::json& json,
      const std::unordered_map<std::string, std::vector<SchemaFieldRule>>&
          nested,
      ValidationResult& result) {
    for (const auto& [prefix, rules] : nested) {
      std::string built_path{};
      const auto* node = resolveNestedPath(json, prefix, built_path);
      if (node != nullptr) {
        validateFields(*node, rules, built_path, result);
      }
    }
  }

}  // namespace

ValidationResult SchemaValidator::validate(const nlohmann::json& json,
                                           std::string_view schema_id,
                                           const SchemaRegistry& registry) {
  ValidationResult result{};
  const auto* def = SchemaRegistry::findSchema(registry, schema_id);
  if (def == nullptr) {
    ValidationResult::addError(
        result, "", ValidationErrorKind::SCHEMA_VERSION_MISMATCH,
        "Schema '" + std::string(schema_id) + "' not found in registry");
    return result;
  }
  if (!json.is_object()) {
    ValidationResult::addError(result, "", ValidationErrorKind::WRONG_TYPE,
                               "Expected JSON object at root");
    return result;
  }
  validateFields(json, def->fields, "", result);
  validateNestedObjects(json, def->nested_objects, result);
  return result;
}

}  // namespace eng::schema
