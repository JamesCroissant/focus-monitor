#include <filesystem>
#include <fstream>

#include "../src/cascade_resolver.h"
#include "test_framework.h"

namespace {

class ScratchDir {
 public:
  ScratchDir() : path_(std::filesystem::temp_directory_path() / "focus_monitor_test_cascade_resolver") {
    std::filesystem::remove_all(path_);
    std::filesystem::create_directories(path_);
  }
  ~ScratchDir() { std::filesystem::remove_all(path_); }

  std::string Touch(const std::string& name) const {
    const std::string full_path = (path_ / name).string();
    std::ofstream(full_path) << "placeholder";
    return full_path;
  }

  std::string Path(const std::string& name) const { return (path_ / name).string(); }

 private:
  std::filesystem::path path_;
};

void ExplicitPathAlwaysWinsEvenIfItDoesNotExist() {
  const ScratchDir dir;
  const std::string result =
      ResolveCascadePath(dir.Path("does_not_exist.xml"), {dir.Touch("real.xml")});
  CHECK(result == dir.Path("does_not_exist.xml"));
}

void ReturnsTheFirstSearchPathThatExists() {
  const ScratchDir dir;
  const std::string second = dir.Touch("second.xml");
  const std::vector<std::string> search_paths = {dir.Path("first_missing.xml"), second,
                                                   dir.Path("third_missing.xml")};
  CHECK(ResolveCascadePath("", search_paths) == second);
}

// Order matters, not just existence: if two candidates both exist, the
// earlier one in the list wins.
void EarlierExistingPathWinsOverALaterOne() {
  const ScratchDir dir;
  const std::string first = dir.Touch("first.xml");
  const std::string second = dir.Touch("second.xml");
  CHECK(ResolveCascadePath("", {first, second}) == first);
}

// None of the search paths exist -- the caller (cv::CascadeClassifier::
// load()) still needs *some* real-looking path to name in its own error
// message, so this falls back to the first entry rather than returning
// empty.
void FallsBackToTheFirstSearchPathWhenNoneExist() {
  const ScratchDir dir;
  const std::vector<std::string> search_paths = {dir.Path("missing_a.xml"), dir.Path("missing_b.xml")};
  CHECK(ResolveCascadePath("", search_paths) == search_paths.front());
}

void EmptySearchPathsWithNoExplicitPathReturnsEmpty() {
  CHECK(ResolveCascadePath("", {}).empty());
}

}  // namespace

int main() {
  RUN_TEST(ExplicitPathAlwaysWinsEvenIfItDoesNotExist);
  RUN_TEST(ReturnsTheFirstSearchPathThatExists);
  RUN_TEST(EarlierExistingPathWinsOverALaterOne);
  RUN_TEST(FallsBackToTheFirstSearchPathWhenNoneExist);
  RUN_TEST(EmptySearchPathsWithNoExplicitPathReturnsEmpty);
  return test_framework::FailureCount() == 0 ? 0 : 1;
}
