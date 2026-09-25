#include <cstdlib>
#include <editor/build/editor-toolchain.h>

namespace eng::editor {

EditorToolchain editorToolchain() {
  EditorToolchain tools{SIMPLISH_CMAKE_COMMAND, SIMPLISH_CMAKE_GENERATOR,
                        SIMPLISH_MAKE_PROGRAM, SIMPLISH_CXX_COMPILER,
                        SIMPLISH_ENGINE_ROOT};
  // NOLINTNEXTLINE(concurrency-mt-unsafe) -- main-thread-only read
  if (const char* root = std::getenv("SIMPLISH_ENGINE_ROOT")) {
    tools.engine_root = root;
  }
  return tools;
}

}  // namespace eng::editor
