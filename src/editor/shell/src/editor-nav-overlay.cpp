#include <editor/shell/editor-nav-overlay.h>

namespace eng::editor {

namespace {

  /// Every run of one kind along row @p row, open ones left out.
  void appendRowRuns(const EditorNavigation& navigation, uint32_t row,
                     std::vector<EditorNavRun>& runs) {
    const uint32_t width = navigation.grid.spec().width;
    const size_t base = static_cast<size_t>(row) * width;
    uint32_t first = 0;
    while (first < width) {
      const EditorNavCell kind = navigation.cells[base + first];
      uint32_t end = first + 1;
      while (end < width && navigation.cells[base + end] == kind) {
        ++end;
      }
      if (kind != EditorNavCell::OPEN) {
        runs.push_back({row, first, end, kind});
      }
      first = end;
    }
  }

}  // namespace

EditorNavOverlay editorNavOverlay(const EditorNavigation& navigation) {
  const spatial::NavGridSpec& spec = navigation.grid.spec();
  EditorNavOverlay overlay{.origin = spec.origin,
                           .cell_size = spec.cell_size,
                           .floor_z = spec.floor_z};
  if (navigation.cells.size() != navigation.grid.cellCount()) {
    return overlay;
  }
  for (uint32_t row = 0; row < spec.height; ++row) {
    appendRowRuns(navigation, row, overlay.runs);
  }
  return overlay;
}

}  // namespace eng::editor
