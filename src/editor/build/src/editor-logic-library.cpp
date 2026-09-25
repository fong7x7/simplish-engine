#include <editor/build/editor-logic-library-load.h>
#include <editor/build/editor-logic-library.h>
#include <engine/core/dynamic-library.h>
#include <game/logic/game-logic-entry.h>
#include <string>
#include <system_error>
#include <utility>

namespace eng::editor {

namespace {

  /// The symbol @p name in @p handle, as a function of type @p Fn.
  template <typename Fn> Fn exported(void* handle, std::string_view name) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) --
    // a symbol's address is only ever a void*
    return reinterpret_cast<Fn>(DynamicLibrary::symbol(handle, name));
  }

  /// Copy @p built to @p load_path, replacing what is there. Why not,
  /// when it cannot.
  std::string copyForLoad(const std::filesystem::path& built,
                          const std::filesystem::path& load_path) {
    std::error_code ec;
    std::filesystem::create_directories(load_path.parent_path(), ec);
    std::filesystem::copy_file(
        built, load_path, std::filesystem::copy_options::overwrite_existing,
        ec);
    return ec ? "Could not copy " + built.string() + ": " + ec.message()
              : std::string{};
  }

  /// Why @p handle is not a game logic library these headers can call:
  /// a missing export, or another API version. Empty when it is one.
  std::string checkExports(void* handle) {
    const auto version = exported<game::GameLogicApiVersionFn>(
        handle, game::GAME_LOGIC_VERSION_SYMBOL);
    if (version == nullptr ||
        exported<game::GameLogicCreateFn>(
            handle, game::GAME_LOGIC_CREATE_SYMBOL) == nullptr ||
        exported<game::GameLogicDestroyFn>(
            handle, game::GAME_LOGIC_DESTROY_SYMBOL) == nullptr) {
      return "The library exports no game logic: is SIMPLISH_GAME_LOGIC "
             "written in one of its sources?";
    }
    if (version() != game::GAME_LOGIC_API_VERSION) {
      return "The library was built against game logic API version " +
             std::to_string(version()) + "; this editor speaks version " +
             std::to_string(game::GAME_LOGIC_API_VERSION) + ". Rebuild it.";
    }
    return {};
  }

  /// A failed load, having closed @p handle and removed @p copy.
  EditorLogicLibraryLoad failed(void* handle, const std::filesystem::path& copy,
                                std::string error) {
    DynamicLibrary::close(handle);
    std::error_code ec;
    std::filesystem::remove(copy, ec);
    return {nullptr, std::move(error)};
  }

}  // namespace

EditorLogicLibraryLoad
loadEditorLogicLibrary(const std::filesystem::path& built,
                       const std::filesystem::path& load_path) {
  if (std::string error = copyForLoad(built, load_path); !error.empty()) {
    return {nullptr, std::move(error)};
  }
  void* handle = DynamicLibrary::open(load_path.string());
  if (handle == nullptr) {
    return failed(nullptr, load_path, DynamicLibrary::lastError());
  }
  if (std::string error = checkExports(handle); !error.empty()) {
    return failed(handle, load_path, std::move(error));
  }
  const game::GameLogicFactory factory{
      exported<game::GameLogicCreateFn>(handle, game::GAME_LOGIC_CREATE_SYMBOL),
      exported<game::GameLogicDestroyFn>(handle,
                                         game::GAME_LOGIC_DESTROY_SYMBOL)};
  return {std::make_shared<EditorLogicLibrary>(handle, factory, load_path), {}};
}

EditorLogicLibrary::EditorLogicLibrary(void* handle,
                                       game::GameLogicFactory factory,
                                       std::filesystem::path path)
  : handle_(handle), factory_(factory), path_(std::move(path)) {}

EditorLogicLibrary::~EditorLogicLibrary() {
  DynamicLibrary::close(handle_);
  std::error_code ec;
  std::filesystem::remove(path_, ec);
}

}  // namespace eng::editor
