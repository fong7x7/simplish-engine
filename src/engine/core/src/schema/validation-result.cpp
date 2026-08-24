#include <algorithm>
#include <engine/core/schema/validation-result.h>
#include <utility>

namespace eng::schema {

bool ValidationResult::hasErrors(const ValidationResult& result) {
  return !result.errors.empty();
}

bool ValidationResult::hasBlockingErrors(const ValidationResult& result) {
  return std::ranges::any_of(result.errors, [](const auto& err) {
    return err.kind == ValidationErrorKind::MISSING_REQUIRED_FIELD ||
           err.kind == ValidationErrorKind::SCHEMA_VERSION_MISMATCH;
  });
}

void ValidationResult::addError(ValidationResult& result,
                                std::string field_path,
                                ValidationErrorKind kind, std::string message) {
  result.errors.push_back({std::move(field_path), kind, std::move(message)});
}

}  // namespace eng::schema
