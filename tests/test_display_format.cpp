#include "../src/display_format.h"
#include "test_framework.h"

namespace {

void FormatsFocusedStatusWithSeconds() {
  CHECK(FormatStatusLine(/*focused=*/true, 12) == "Focused (12s)");
}

void FormatsNotFocusedStatusWithSeconds() {
  CHECK(FormatStatusLine(/*focused=*/false, 3) == "Not focused (3s)");
}

void FormatsZeroSecondsInState() {
  CHECK(FormatStatusLine(/*focused=*/true, 0) == "Focused (0s)");
}

// Multi-digit second counts shouldn't be truncated or reformatted --
// a long session's status line should read exactly as the number is.
void FormatsMultiDigitSeconds() {
  CHECK(FormatStatusLine(/*focused=*/false, 12345) == "Not focused (12345s)");
}

void FormatsZeroPercentRatio() {
  CHECK(FormatRatioLine(0.0) == "Focus rate: 0%");
}

void FormatsFullPercentRatio() {
  CHECK(FormatRatioLine(1.0) == "Focus rate: 100%");
}

// Truncates toward the nearest whole percent *below* the true value --
// 87.6% reads as "87%", not rounded up to "88%". This matches the
// convention of never showing a round number the session hasn't
// actually reached yet (same reason a progress bar doesn't round up to
// 100% before the work is actually done).
void TruncatesRatherThanRoundsThePercentage() {
  CHECK(FormatRatioLine(0.876) == "Focus rate: 87%");
  CHECK(FormatRatioLine(0.129) == "Focus rate: 12%");
}

}  // namespace

int main() {
  RUN_TEST(FormatsFocusedStatusWithSeconds);
  RUN_TEST(FormatsNotFocusedStatusWithSeconds);
  RUN_TEST(FormatsZeroSecondsInState);
  RUN_TEST(FormatsMultiDigitSeconds);
  RUN_TEST(FormatsZeroPercentRatio);
  RUN_TEST(FormatsFullPercentRatio);
  RUN_TEST(TruncatesRatherThanRoundsThePercentage);
  return test_framework::FailureCount() == 0 ? 0 : 1;
}
