#include "../src/consecutive_failure_guard.h"
#include "test_framework.h"

namespace {

void DoesNotGiveUpBeforeThreshold() {
  ConsecutiveFailureGuard guard(3);
  CHECK(!guard.RecordFailure());
  CHECK(!guard.RecordFailure());
}

void GivesUpOnceThresholdReached() {
  ConsecutiveFailureGuard guard(3);
  CHECK(!guard.RecordFailure());
  CHECK(!guard.RecordFailure());
  CHECK(guard.RecordFailure());
}

// A single failed attempt is normal and expected to recover -- the whole
// point of this class is to distinguish that from a permanent failure,
// so a success in between must reset the count rather than letting
// failures accumulate across it.
void SuccessResetsTheCount() {
  ConsecutiveFailureGuard guard(3);
  guard.RecordFailure();
  guard.RecordFailure();
  guard.RecordSuccess();
  CHECK(!guard.RecordFailure());
  CHECK(!guard.RecordFailure());
  CHECK(guard.RecordFailure());
}

void ThresholdOfOneGivesUpOnTheFirstFailure() {
  ConsecutiveFailureGuard guard(1);
  CHECK(guard.RecordFailure());
}

// Once given up, further failures must keep reporting true too (not
// strictly required by any caller today, but nothing about the class
// itself should silently stop reporting true past the threshold).
void StaysGivenUpOnFurtherFailures() {
  ConsecutiveFailureGuard guard(2);
  CHECK(!guard.RecordFailure());
  CHECK(guard.RecordFailure());
  CHECK(guard.RecordFailure());
}

}  // namespace

int main() {
  RUN_TEST(DoesNotGiveUpBeforeThreshold);
  RUN_TEST(GivesUpOnceThresholdReached);
  RUN_TEST(SuccessResetsTheCount);
  RUN_TEST(ThresholdOfOneGivesUpOnTheFirstFailure);
  RUN_TEST(StaysGivenUpOnFurtherFailures);
  return test_framework::FailureCount() == 0 ? 0 : 1;
}
