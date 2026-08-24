#pragma once

#include "audit-emit-params.h"
#include "audit-emit-result.h"
#include "audit-emit-toggle.h"
#include "audit-event.h"
#include "audit-payload-fn.h"

namespace eng {

void setAuditEmitEnabled(AuditEmitToggle toggle);

bool isAuditEmitEnabled();

AuditEmitResult emitAuditEvent(const AuditEmitParams& params,
                               AuditPayloadFn payload_fn, void* user_data);

AuditEmitResult emitAuditEventCausedBy(uint32_t cause_offset,
                                       const AuditEmitParams& params,
                                       AuditPayloadFn payload_fn,
                                       void* user_data);

AuditEmitResult emitAuditEventError(uint32_t error_code,
                                    const AuditEmitParams& params,
                                    AuditPayloadFn payload_fn, void* user_data);

}  // namespace eng

#if defined(ENGINE_AUDIT) && ENGINE_AUDIT == 0

// NOLINTBEGIN(cppcoreguidelines-avoid-do-while) -- do-while is the standard
// multi-statement macro idiom
#define AUDIT_EVENT(category, event_type, severity, actor_id, target_id,       \
                    flags, payload_lambda)                                     \
  do {                                                                         \
  } while (false)

#define AUDIT_EVENT_CAUSED_BY(cause_offset, category, event_type, severity,    \
                              actor_id, target_id, flags, payload_lambda)      \
  do {                                                                         \
  } while (false)

#define AUDIT_EVENT_ERROR(error_code, category, event_type, severity,          \
                          actor_id, target_id, flags, payload_lambda)          \
  do {                                                                         \
  } while (false)
// NOLINTEND(cppcoreguidelines-avoid-do-while)

#else

// NOLINTBEGIN(cppcoreguidelines-macro-usage,cppcoreguidelines-avoid-do-while)

#define AUDIT_EVENT(category, event_type, severity, actor_id, target_id,       \
                    flags, payload_lambda)                                     \
  do {                                                                         \
    auto audit_payload_fn_ = (payload_lambda);                                 \
    struct AuditPayloadAdapter_ {                                              \
      decltype(audit_payload_fn_) fn;                                          \
      static void invoke(::eng::AuditPayloadWriter& w, void* ud) {             \
        auto* self = static_cast<AuditPayloadAdapter_*>(ud);                   \
        self->fn(w);                                                           \
      }                                                                        \
    } audit_adapter_{audit_payload_fn_};                                       \
    ::eng::emitAuditEvent({(category), (event_type), (severity), (actor_id),   \
                           (target_id), (flags)},                              \
                          &AuditPayloadAdapter_::invoke, &audit_adapter_);     \
  } while (false)

#define AUDIT_EVENT_CAUSED_BY(cause_offset, category, event_type, severity,    \
                              actor_id, target_id, flags, payload_lambda)      \
  do {                                                                         \
    auto audit_payload_fn_ = (payload_lambda);                                 \
    struct AuditPayloadAdapter_ {                                              \
      decltype(audit_payload_fn_) fn;                                          \
      static void invoke(::eng::AuditPayloadWriter& w, void* ud) {             \
        auto* self = static_cast<AuditPayloadAdapter_*>(ud);                   \
        self->fn(w);                                                           \
      }                                                                        \
    } audit_adapter_{audit_payload_fn_};                                       \
    ::eng::emitAuditEventCausedBy((cause_offset),                              \
                                  {(category), (event_type), (severity),       \
                                   (actor_id), (target_id), (flags)},          \
                                  &AuditPayloadAdapter_::invoke,               \
                                  &audit_adapter_);                            \
  } while (false)

#define AUDIT_EVENT_ERROR(error_code, category, event_type, severity,          \
                          actor_id, target_id, flags, payload_lambda)          \
  do {                                                                         \
    auto audit_payload_fn_ = (payload_lambda);                                 \
    struct AuditPayloadAdapter_ {                                              \
      decltype(audit_payload_fn_) fn;                                          \
      static void invoke(::eng::AuditPayloadWriter& w, void* ud) {             \
        auto* self = static_cast<AuditPayloadAdapter_*>(ud);                   \
        self->fn(w);                                                           \
      }                                                                        \
    } audit_adapter_{audit_payload_fn_};                                       \
    ::eng::emitAuditEventError((error_code),                                   \
                               {(category), (event_type), (severity),          \
                                (actor_id), (target_id), (flags)},             \
                               &AuditPayloadAdapter_::invoke,                  \
                               &audit_adapter_);                               \
  } while (false)

// NOLINTEND(cppcoreguidelines-macro-usage,cppcoreguidelines-avoid-do-while)

#endif
