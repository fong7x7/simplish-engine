#include "support/build-temp-dir.h"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <editor/build/editor-build-job.h>
#include <editor/build/editor-build-log.h>
#include <editor/build/editor-toolchain.h>
#include <thread>

using namespace eng::editor;

namespace {

/// Wait for @p job to finish, and say how it did.
EditorBuildStatus finish(EditorBuildJob& job) {
  while (job.status() == EditorBuildStatus::RUNNING) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return job.status();
}

/// `cmake -E <words>`: a portable command every build machine has.
EditorBuildCommand cmakeE(std::vector<std::string> words) {
  words.insert(words.begin(), {editorToolchain().cmake, "-E"});
  return {words};
}

}  // namespace

TEST_CASE("a build job runs its commands in order, into its log") {
  const test::BuildTempDir dir("job");
  const auto log = dir.path() / "b.log";
  EditorBuildJob job;

  REQUIRE(job.start({cmakeE({"echo", "first"}), cmakeE({"echo", "second"})},
                    log));
  REQUIRE(finish(job) == EditorBuildStatus::SUCCEEDED);
  const auto lines = readLogLines(log);
  REQUIRE(std::find(lines.begin(), lines.end(), "first") < 
          std::find(lines.begin(), lines.end(), "second"));
}

TEST_CASE("a build job stops at the first command that fails") {
  const test::BuildTempDir dir("job-fails");
  const auto log = dir.path() / "b.log";
  EditorBuildJob job;

  REQUIRE(job.start({cmakeE({"false"}), cmakeE({"echo", "never"})}, log));
  REQUIRE(finish(job) == EditorBuildStatus::FAILED);
  const auto lines = readLogLines(log);
  REQUIRE(std::find(lines.begin(), lines.end(), "never") == lines.end());
}

TEST_CASE("a build job has done nothing before it is started") {
  EditorBuildJob job;

  REQUIRE(job.status() == EditorBuildStatus::IDLE);
}

TEST_CASE("a build job being destroyed does not wait for its build") {
  const test::BuildTempDir dir("job-abandoned");
  const auto started = std::chrono::steady_clock::now();
  {
    EditorBuildJob job;
    REQUIRE(job.start({cmakeE({"sleep", "3"})}, dir.path() / "b.log"));
  }

  REQUIRE(std::chrono::steady_clock::now() - started <
          std::chrono::seconds(2));
}
