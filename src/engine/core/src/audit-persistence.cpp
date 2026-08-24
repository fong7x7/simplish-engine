#include <algorithm>
#include <engine/core/audit/audit-persistence.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

namespace eng {

/// Opaque file handle wrapping a std::ofstream.
struct AuditFileHandle {
  /// Output stream for the current audit file.
  std::ofstream stream;
};

/// Opaque reader state wrapping a std::ifstream.
struct AuditFileReaderImpl {
  /// Input stream for the file being read.
  std::ifstream stream;
};

// Special members defined here where AuditFileHandle is complete (pimpl).
AuditPersistenceContext::AuditPersistenceContext() = default;
AuditPersistenceContext::~AuditPersistenceContext() = default;
AuditPersistenceContext::AuditPersistenceContext(
    AuditPersistenceContext&&) noexcept = default;
AuditPersistenceContext& AuditPersistenceContext::operator=(
    AuditPersistenceContext&&) noexcept = default;

// Special members defined here where AuditFileReaderImpl is complete (pimpl).
AuditFileReader::AuditFileReader() = default;
AuditFileReader::~AuditFileReader() = default;
AuditFileReader::AuditFileReader(AuditFileReader&&) noexcept = default;
AuditFileReader&
AuditFileReader::operator=(AuditFileReader&&) noexcept = default;

/// Bytes per megabyte for disk budget calculations.
constexpr uint64_t BYTES_PER_MB = 1024 * 1024;

namespace {

  /// Ensure the output directory exists, creating it if needed.
  bool ensureDirectory(const std::string& path) {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    return !ec;
  }

  /// Open a binary output stream at the given path.
  std::unique_ptr<AuditFileHandle> openBinaryFile(const std::string& path) {
    auto handle = std::make_unique<AuditFileHandle>();
    handle->stream.open(path, std::ios::binary | std::ios::out);
    if (!handle->stream.is_open()) {
      return nullptr;
    }
    return handle;
  }

}  // namespace

std::optional<AuditPersistenceContext>
createPersistenceContext(const AuditConfig& config) {
  if (!config.persist_to_disk) {
    return std::nullopt;
  }
  AuditPersistenceContext ctx;
  ctx.output_directory = config.output_directory;
  ctx.max_disk_bytes = static_cast<uint64_t>(config.max_disk_mb) * BYTES_PER_MB;
  ctx.flush_threshold = config.flush_threshold;
  return ctx;
}

bool openAuditFile(AuditPersistenceContext& ctx,
                   const AuditSessionMetadata& /*metadata*/) {
  if (!ensureDirectory(ctx.output_directory)) {
    return false;
  }
  ctx.current_file = openBinaryFile(ctx.output_directory + "/audit.bin");
  ctx.current_file_size = 0;
  return ctx.current_file != nullptr;
}

bool flushToDisk(AuditPersistenceContext& ctx,
                 std::span<const std::byte> raw_events) {
  if (ctx.current_file == nullptr) {
    return false;
  }
  ctx.current_file->stream.write(
      reinterpret_cast<const char*>(
          raw_events
              .data()),  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                         // -- binary I/O
      static_cast<std::streamsize>(raw_events.size()));
  ctx.current_file_size += raw_events.size();
  ctx.total_disk_usage += raw_events.size();
  return ctx.current_file->stream.good();
}

void closeAuditFile(AuditPersistenceContext& ctx) {
  if (ctx.current_file == nullptr) {
    return;
  }
  ctx.current_file->stream.close();
  ctx.current_file.reset();
}

namespace {

  /// Entry for a file eligible for pruning.
  struct PruneEntry {
    /// Filesystem path of the file.
    std::filesystem::path path;
    /// Size of the file in bytes.
    uint64_t size;
    /// Last modification time.
    std::filesystem::file_time_type mtime;
  };

  /// Collect regular files in a directory with their sizes and mtimes.
  std::vector<PruneEntry> collectAuditFiles(const std::string& dir) {
    std::vector<PruneEntry> entries;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
      if (!entry.is_regular_file(ec)) {
        continue;
      }
      const auto size = entry.file_size(ec);
      const auto mtime = entry.last_write_time(ec);
      entries.push_back({entry.path(), size, mtime});
    }
    return entries;
  }

  /// Compute total size of all files in the list.
  uint64_t computeTotalSize(const std::vector<PruneEntry>& files) {
    uint64_t total = 0;
    for (const auto& f : files) {
      total += f.size;
    }
    return total;
  }

  /// Delete oldest files until total_usage fits within budget.
  void deleteOverBudget(const std::vector<PruneEntry>& files,
                        uint64_t& total_usage, uint64_t max_bytes) {
    for (const auto& f : files) {
      if (total_usage <= max_bytes) {
        break;
      }
      std::error_code ec;
      std::filesystem::remove(f.path, ec);
      if (!ec) {
        total_usage -= f.size;
      }
    }
  }

}  // namespace

void pruneOldFiles(AuditPersistenceContext& ctx) {
  if (ctx.output_directory.empty()) {
    return;
  }
  auto files = collectAuditFiles(ctx.output_directory);
  std::ranges::sort(files, [](const PruneEntry& a, const PruneEntry& b) {
    return a.mtime < b.mtime;
  });
  ctx.total_disk_usage = computeTotalSize(files);
  deleteOverBudget(files, ctx.total_disk_usage, ctx.max_disk_bytes);
}

std::optional<AuditFileReader> openAuditFileForRead(std::string_view path) {
  if (!std::filesystem::exists(path)) {
    return std::nullopt;
  }
  auto impl = std::make_unique<AuditFileReaderImpl>();
  impl->stream.open(std::string(path), std::ios::binary);
  if (!impl->stream.is_open()) {
    return std::nullopt;
  }
  AuditFileReader reader;
  reader.path = std::string(path);
  reader.impl = std::move(impl);
  return reader;
}

std::optional<std::vector<std::byte>> readNextBlock(AuditFileReader& reader) {
  auto* impl = reader.impl.get();
  if (impl == nullptr || !impl->stream.good()) {
    return std::nullopt;
  }
  std::vector<std::byte> block(AUDIT_BLOCK_SIZE);
  impl->stream.read(
      reinterpret_cast<
          char*>(         // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
          block.data()),  // -- binary I/O
      static_cast<std::streamsize>(block.size()));
  const auto bytes_read = static_cast<size_t>(impl->stream.gcount());
  if (bytes_read == 0) {
    return std::nullopt;
  }
  block.resize(bytes_read);
  return block;
}

void closeAuditFileReader(AuditFileReader& reader) {
  reader.impl.reset();
}

void shutdownPersistence(AuditPersistenceContext& ctx) {
  closeAuditFile(ctx);
}

}  // namespace eng
