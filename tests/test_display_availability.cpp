#include "../src/display_availability.h"
#include "test_framework.h"

namespace {

void NeitherSetIsUnavailable() {
  CHECK(!DisplayAvailable(nullptr, nullptr));
}

void EmptyStringsCountAsUnset() {
  // std::getenv("DISPLAY") can't return an empty string for a variable
  // that's genuinely unset (it returns nullptr instead) -- but a
  // variable that's explicitly set to an empty value (`DISPLAY=`) is a
  // real, if unusual, shell state, and is just as useless to
  // cv::namedWindow() as not being set at all.
  CHECK(!DisplayAvailable("", ""));
  CHECK(!DisplayAvailable("", nullptr));
  CHECK(!DisplayAvailable(nullptr, ""));
}

void X11DisplayAloneIsAvailable() {
  CHECK(DisplayAvailable(":0", nullptr));
}

void WaylandDisplayAloneIsAvailable() {
  CHECK(DisplayAvailable(nullptr, "wayland-0"));
}

void BothSetIsAvailable() {
  CHECK(DisplayAvailable(":0", "wayland-0"));
}

}  // namespace

int main() {
  RUN_TEST(NeitherSetIsUnavailable);
  RUN_TEST(EmptyStringsCountAsUnset);
  RUN_TEST(X11DisplayAloneIsAvailable);
  RUN_TEST(WaylandDisplayAloneIsAvailable);
  RUN_TEST(BothSetIsAvailable);
  return test_framework::FailureCount() == 0 ? 0 : 1;
}
