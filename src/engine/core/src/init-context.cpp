#include <algorithm>
#include <array>
#include <engine/core/init-context.h>
#include <string_view>
#include <utility>

namespace eng {

namespace {

  /// Entry mapping a phase name to a pointer-to-member for its ready flag.
  struct PhaseEntry {
    /// Phase name constant.
    std::string_view name;
    /// Pointer to the corresponding ready flag in InitContext.
    bool InitContext::* flag;
  };

  /// Lookup table mapping phase names to their ready flags.
  constexpr std::array<PhaseEntry, 15> PHASE_TABLE = {{
      {PHASE_FOUNDATION, &InitContext::foundation_ready},
      {PHASE_CORE_DATA, &InitContext::core_data_ready},
      {PHASE_WORLD_STORAGE, &InitContext::world_storage_ready},
      {PHASE_VOXEL_SYSTEM, &InitContext::voxel_system_ready},
      {PHASE_PHYSICS, &InitContext::physics_ready},
      {PHASE_RENDERING, &InitContext::rendering_ready},
      {PHASE_AUDIO, &InitContext::audio_ready},
      {PHASE_INPUT, &InitContext::input_ready},
      {PHASE_GUI, &InitContext::gui_ready},
      {PHASE_NETWORKING, &InitContext::networking_ready},
      {PHASE_DAY_NIGHT, &InitContext::day_night_ready},
      {PHASE_TRIGGERS, &InitContext::triggers_ready},
      {PHASE_PLUGINS, &InitContext::plugins_ready},
      {PHASE_WORLD_LOAD, &InitContext::world_loaded},
      {PHASE_SIMULATION, &InitContext::simulation_ready},
  }};

}  // namespace

bool InitContext::isPhaseReady(std::string_view phase_name) const {
  const auto* const it =
      std::find_if(  // NOLINT(modernize-use-ranges,llvm-use-ranges)
          PHASE_TABLE.begin(), PHASE_TABLE.end(),
          [&](const PhaseEntry& e) { return e.name == phase_name; });
  if (it == PHASE_TABLE.end()) {
    return false;
  }
  return this->*(it->flag);
}

}  // namespace eng
