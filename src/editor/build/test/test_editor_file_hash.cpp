#include "support/build-temp-dir.h"

#include <catch2/catch_test_macros.hpp>
#include <editor/build/editor-file-hash.h>
#include <editor/build/editor-logic-source.h>
#include <editor/project/project-text-file.h>

using namespace eng::editor;
using eng::editor::test::BuildTempDir;

namespace {

bool everything(const std::string& /*relative*/) {
  return true;
}

bool onlyLevels(const std::string& relative) {
  return relative.starts_with("levels/");
}

/// A folder holding one level and one note.
void writeTree(const std::filesystem::path& root, const std::string& level) {
  (void)writeProjectTextFile(root / "levels" / "a.json", level);
  (void)writeProjectTextFile(root / "notes.txt", "hello");
}

}  // namespace

TEST_CASE("two folders holding the same files hash the same") {
  const BuildTempDir a("hash-a");
  const BuildTempDir b("hash-b");
  writeTree(a.path(), "{}");
  writeTree(b.path(), "{}");
  CHECK(hashFileTree(a.path(), everything) ==
        hashFileTree(b.path(), everything));
}

TEST_CASE("a changed byte or a renamed file changes the hash") {
  const BuildTempDir a("hash-bytes");
  writeTree(a.path(), "{}");
  const uint64_t before = hashFileTree(a.path(), everything);
  writeTree(a.path(), "{ }");
  CHECK(hashFileTree(a.path(), everything) != before);
  const BuildTempDir b("hash-name");
  (void)writeProjectTextFile(b.path() / "levels" / "b.json", "{}");
  (void)writeProjectTextFile(b.path() / "notes.txt", "hello");
  writeTree(a.path(), "{}");
  CHECK(hashFileTree(b.path(), everything) !=
        hashFileTree(a.path(), everything));
}

TEST_CASE("hidden files, and files the filter leaves out, are not hashed") {
  const BuildTempDir a("hash-hidden");
  writeTree(a.path(), "{}");
  const uint64_t levels = hashFileTree(a.path(), onlyLevels);
  const uint64_t all = hashFileTree(a.path(), everything);
  (void)writeProjectTextFile(a.path() / ".DS_Store", "finder");
  (void)writeProjectTextFile(a.path() / "levels" / ".a.json.swp", "vim");
  (void)writeProjectTextFile(a.path() / "notes.txt", "changed");
  CHECK(hashFileTree(a.path(), onlyLevels) == levels);
  CHECK(hashFileTree(a.path(), everything) != all);
  CHECK(editorPathHidden(".git/HEAD"));
  CHECK_FALSE(editorPathHidden("src/game.cpp"));
}

TEST_CASE("a project's logic hash follows its src/ and nothing else") {
  const BuildTempDir root("hash-logic");
  (void)writeProjectTextFile(root.path() / "src" / "game.cpp", "int a;");
  const uint64_t before = projectLogicHash(root.path());
  (void)writeProjectTextFile(root.path() / "content" / "x.json", "{}");
  CHECK(projectLogicHash(root.path()) == before);
  (void)writeProjectTextFile(root.path() / "src" / "game.cpp", "int b;");
  CHECK(projectLogicHash(root.path()) != before);
}
