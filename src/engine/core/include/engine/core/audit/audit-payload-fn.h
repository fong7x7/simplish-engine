#pragma once

#include <engine/core/audit/audit-event.h>

namespace eng {

using AuditPayloadFn = void (*)(AuditPayloadWriter& writer, void* user_data);

}  // namespace eng
