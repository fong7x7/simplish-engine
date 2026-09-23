#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-sound-import.h>
#include <filesystem>
#include <fstream>

using namespace eng::editor;
namespace fs = std::filesystem;

namespace {

/// Write a tiny valid mono 16-bit WAV to @p path.
void writeWav(const fs::path& path) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  const unsigned char header[] = {
      'R',  'I',  'F', 'F', 40, 0,    0, 0, 'W', 'A',  'V', 'E',
      'f',  'm',  't', ' ', 16, 0,    0, 0, 1,   0,    1,   0,
      0x80, 0xBB, 0,   0,   0,  0x77, 1, 0, 2,   0,    16,  0,
      'd',  'a',  't', 'a', 4,  0,    0, 0, 0,   0x40, 0,   0x40};
  out.write(reinterpret_cast<const char*>(header), sizeof(header));
}

/// A fresh directory for one test, gone again after.
struct Scratch {
  fs::path root;
  explicit Scratch(const char* name) : root(fs::temp_directory_path() / name) {
    fs::remove_all(root);
  }
  ~Scratch() { fs::remove_all(root); }
  Scratch(const Scratch&) = delete;
  Scratch& operator=(const Scratch&) = delete;
};

}  // namespace

TEST_CASE("an imported sound is copied into sounds/ under its own name") {
  const Scratch scratch("simplish-import-copy");
  writeWav(scratch.root / "outside" / "boom.wav");
  const fs::path assets = scratch.root / "assets";
  const EditorSoundImport imported =
      importEditorSound(assets, scratch.root / "outside" / "boom.wav");
  REQUIRE(imported.error.empty());
  REQUIRE(imported.file == fs::path("sounds") / "boom.wav");
  REQUIRE(fs::exists(assets / "sounds" / "boom.wav"));
}

TEST_CASE("an import never overwrites a file the project has") {
  const Scratch scratch("simplish-import-rename");
  writeWav(scratch.root / "outside" / "boom.wav");
  const fs::path assets = scratch.root / "assets";
  const fs::path source = scratch.root / "outside" / "boom.wav";
  REQUIRE(importEditorSound(assets, source).file.filename() == "boom.wav");
  REQUIRE(importEditorSound(assets, source).file.filename() == "boom-2.wav");
  REQUIRE(importEditorSound(assets, source).file.filename() == "boom-3.wav");
}

TEST_CASE("a sound already under assets/ is used where it is") {
  const Scratch scratch("simplish-import-inside");
  writeWav(scratch.root / "assets" / "fx" / "tick.wav");
  const EditorSoundImport imported = importEditorSound(
      scratch.root / "assets", scratch.root / "assets" / "fx" / "tick.wav");
  REQUIRE(imported.error.empty());
  REQUIRE(imported.file == fs::path("fx") / "tick.wav");
  REQUIRE_FALSE(fs::exists(scratch.root / "assets" / "sounds"));
}

TEST_CASE("what is not a playable sound is refused before anything is copied") {
  const Scratch scratch("simplish-import-refuse");
  fs::create_directories(scratch.root);
  std::ofstream(scratch.root / "notes.txt") << "hello";
  std::ofstream(scratch.root / "fake.wav") << "not a wav";
  const fs::path assets = scratch.root / "assets";
  REQUIRE_FALSE(
      importEditorSound(assets, scratch.root / "notes.txt").error.empty());
  REQUIRE_FALSE(
      importEditorSound(assets, scratch.root / "fake.wav").error.empty());
  REQUIRE_FALSE(
      importEditorSound(assets, scratch.root / "gone.ogg").error.empty());
  REQUIRE_FALSE(fs::exists(assets));
}
