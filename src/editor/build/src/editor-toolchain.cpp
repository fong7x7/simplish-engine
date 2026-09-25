#include <cstdlib>
#include <editor/build/editor-toolchain.h>

// Defined by src/bin/logic-check when the checker is built beside the
// editor; without it, a build is loaded unchecked.
#ifndef SIMPLISH_LOGIC_CHECK
#define SIMPLISH_LOGIC_CHECK ""
#endif

namespace eng::editor {

EditorToolchain editorToolchain() {
  EditorToolchain tools{SIMPLISH_CMAKE_COMMAND, SIMPLISH_CMAKE_GENERATOR,
                        SIMPLISH_MAKE_PROGRAM, SIMPLISH_CXX_COMPILER,
                        SIMPLISH_ENGINE_ROOT, SIMPLISH_LOGIC_CHECK};
  // NOLINTNEXTLINE(concurrency-mt-unsafe) -- main-thread-only read
  if (const char* root = std::getenv("SIMPLISH_ENGINE_ROOT")) {
    tools.engine_root = root;
  }
  return tools;
}

}  // namespace eng::editor
