#include <algorithm>
#include <array>
#include <editor/shell/editor-behavior-choices.h>
#include <editor/shell/editor-entity-id.h>
#include <game/content/behavior-lookup.h>

namespace eng::editor {

namespace {

  /// What a behavior reference starts with.
  constexpr std::string_view BEHAVIOR_PREFIX = "behavior:";

  /// What the Faction row calls each faction, in enumerator order.
  constexpr std::array<std::string_view, 3> FACTION_NAMES{"Hostile", "Neutral",
                                                          "Friendly"};
  static_assert(FACTION_NAMES.size() == game::ALL_FACTIONS.size());

  /// The one of @p behaviors called @p id, if any.
  const game::BehaviorDefinition*
  withId(const std::vector<game::BehaviorDefinition>& behaviors,
         std::string_view id) {
    for (const game::BehaviorDefinition& behavior : behaviors) {
      if (behavior.id == id) {
        return &behavior;
      }
    }
    return nullptr;
  }

  /// Whether some built-in behavior is called @p id.
  bool isBuiltIn(std::string_view id) {
    return std::ranges::any_of(game::builtInBehaviors(),
                               [id](const game::BehaviorDefinition& behavior) {
                                 return behavior.id == id;
                               });
  }

  /// Offer @p behavior, which names nothing the project can run, last in
  /// @p choices, and pick it: the row says what the prop holds.
  void offerMissing(EditorBehaviorChoices& choices,
                    const std::string& behavior) {
    choices.names.push_back(behavior + " (missing)");
    choices.refs.push_back(behavior);
    choices.current = choices.refs.size() - 1U;
  }

}  // namespace

std::vector<const game::BehaviorDefinition*>
editorAvailableBehaviors(const std::vector<game::BehaviorDefinition>& project) {
  std::vector<const game::BehaviorDefinition*> available;
  for (const game::BehaviorDefinition& preset : game::builtInBehaviors()) {
    const game::BehaviorDefinition* own = withId(project, preset.id);
    available.push_back(own != nullptr ? own : &preset);
  }
  for (const game::BehaviorDefinition& own : project) {
    if (!isBuiltIn(own.id)) {
      available.push_back(&own);
    }
  }
  return available;
}

EditorBehaviorChoices
editorBehaviorChoices(const std::vector<game::BehaviorDefinition>& project,
                      const std::string& behavior) {
  EditorBehaviorChoices choices;
  choices.names.emplace_back(EDITOR_BEHAVIOR_NONE_NAME);
  choices.refs.emplace_back();
  for (const game::BehaviorDefinition* each :
       editorAvailableBehaviors(project)) {
    choices.names.push_back(each->name);
    choices.refs.push_back(editorBehaviorRef(each->id));
    if (choices.refs.back() == behavior) {
      choices.current = choices.refs.size() - 1U;
    }
  }
  if (choices.current == 0 && !behavior.empty()) {
    offerMissing(choices, behavior);
  }
  return choices;
}

std::string editorBehaviorRef(std::string_view id) {
  return editorQualifiedId(EditorIdKind::BEHAVIOR, id);
}

std::string editorBehaviorIdOf(std::string_view ref) {
  return ref.starts_with(BEHAVIOR_PREFIX)
             ? std::string(ref.substr(BEHAVIOR_PREFIX.size()))
             : std::string{};
}

const game::BehaviorDefinition*
findEditorBehavior(const std::vector<game::BehaviorDefinition>& project,
                   std::string_view ref) {
  const std::string id = editorBehaviorIdOf(ref);
  for (const game::BehaviorDefinition* each :
       editorAvailableBehaviors(project)) {
    if (!id.empty() && each->id == id) {
      return each;
    }
  }
  return nullptr;
}

std::string_view editorFactionName(game::Faction faction) {
  return FACTION_NAMES[static_cast<size_t>(faction)];
}

std::vector<std::string> editorFactionNames() {
  return {FACTION_NAMES.begin(), FACTION_NAMES.end()};
}

}  // namespace eng::editor
