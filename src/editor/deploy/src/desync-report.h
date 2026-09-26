#pragma once

/// @file desync-report.h
/// @brief What a server writes when a co-op run desyncs.
/// @par Threading Pure functions, but for writing the file.

#include <engine/net/net-desync.h>
#include <engine/net/net-peer-trace.h>
#include <engine/net/net-trace-divergence.h>
#include <filesystem>
#include <optional>
#include <span>
#include <string>

namespace eng::editor {

/// Ticks either side of the first divergence the report tabulates.
inline constexpr uint64_t DESYNC_REPORT_SPAN = 4;

/// Where a desync of @p level caught at @p tick is reported, in @p dir —
/// the working directory when empty.
[[nodiscard]] std::filesystem::path
desyncReportPath(const std::filesystem::path& dir, const std::string& level,
                 uint64_t tick);

/// The traces' verdict in a sentence: where the run first diverged, and
/// between whom; or that it diverged before the traces reach.
[[nodiscard]] std::string
divergenceText(const std::optional<net::NetTraceDivergence>& divergence);

/// The report of @p desync in a run of @p level: the verdict of
/// @p traces, which peers sent what, every peer's hash of each tick around
/// the first divergence, and each section's hash on it.
[[nodiscard]] std::string
desyncReport(const std::string& level, const net::NetDesync& desync,
             std::span<const net::NetPeerTrace> traces);

}  // namespace eng::editor
