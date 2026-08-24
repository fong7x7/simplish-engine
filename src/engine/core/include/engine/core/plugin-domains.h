#pragma once

// Barrel: typed C++ wrappers for PluginAPI domains.
// Technical Approach: docs/technical-approaches/engine/plugin-cpp-wrappers.md
//
// NOLINTBEGIN(bugprone-suspicious-stringview-data-usage)
// string_view parameters require null-terminated backing storage (see leaf
// headers).

#include "audio-domain-wrapper.h"
#include "entity-domain-wrapper.h"
#include "input-domain-wrapper.h"
#include "physics-domain-wrapper.h"
#include "plugin-async-api.h"
#include "plugin-event-api.h"
#include "plugin-log-api.h"
#include "render-domain-wrapper.h"
#include "voxel-domain-wrapper.h"
#include "world-domain-wrapper.h"

// NOLINTEND(bugprone-suspicious-stringview-data-usage)
