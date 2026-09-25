#include <editor/build/editor-logic-test.h>
#include <nlohmann/json.hpp>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// The string at @p key of @p object, or empty.
  std::string text(const json& object, std::string_view key) {
    const auto found = object.find(key);
    return found != object.end() && found->is_string()
               ? found->get<std::string>()
               : std::string{};
  }

  /// The whole number at @p key of @p object, or 0.
  uint64_t count(const json& object, std::string_view key) {
    const auto found = object.find(key);
    return found != object.end() && found->is_number_unsigned()
               ? found->get<uint64_t>()
               : 0;
  }

  /// One failure, read back.
  EditorBuildDiagnostic failure(const json& entry) {
    return {text(entry, "file"), static_cast<uint32_t>(count(entry, "line")),
            0, EditorDiagnosticSeverity::ERROR, text(entry, "message")};
  }

  /// One test, read back.
  EditorLogicTest test(const json& entry) {
    EditorLogicTest read{text(entry, "name"), text(entry, "level"), false,
                         count(entry, "ticks"), {}};
    const auto passed = entry.find("passed");
    read.passed = passed != entry.end() && passed->is_boolean() &&
                  passed->get<bool>();
    const auto failures = entry.find("failures");
    for (size_t i = 0; failures != entry.end() && failures->is_array() &&
                       i < failures->size();
         ++i) {
      read.failures.push_back(failure((*failures)[i]));
    }
    return read;
  }

}  // namespace

std::vector<EditorLogicTest> parseLogicTestResults(std::string_view text_in) {
  const json root = json::parse(text_in, nullptr, false);
  std::vector<EditorLogicTest> tests;
  for (size_t i = 0; root.is_array() && i < root.size(); ++i) {
    if (root[i].is_object()) {
      tests.push_back(test(root[i]));
    }
  }
  return tests;
}

}  // namespace eng::editor
