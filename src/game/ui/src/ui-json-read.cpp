#include "ui-json-read.h"

namespace eng::game {

void uiProblem(UiJsonRead& read, std::string_view path, std::string_view what) {
  read.problems.push_back(std::string(path) + ": " + std::string(what));
}

std::string uiText(const nlohmann::json& object, std::string_view key) {
  const auto found = object.find(key);
  return found != object.end() && found->is_string() ? found->get<std::string>()
                                                     : std::string{};
}

std::optional<float> uiFloat(const nlohmann::json& object,
                             std::string_view key) {
  const auto found = object.find(key);
  return found != object.end() && found->is_number()
             ? std::optional(found->get<float>())
             : std::nullopt;
}

}  // namespace eng::game
