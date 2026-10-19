#include "../src/alert_scheduler.h"
#include "test_framework.h"

namespace {

using Clock = AlertScheduler::Clock;

Clock::time_point T(double seconds) {
  return Clock::time_point{} + std::chrono::duration_cast<Clock::duration>(
                                   std::chrono::duration<double>(seconds));
}

void NeverAlertsWhileFocused() {
  AlertScheduler scheduler(std::chrono::seconds(10), std::chrono::seconds(30));
  CHECK(!scheduler.ShouldAlert(/*focused=*/true, T(0), T(100)));
}

void DoesNotAlertBeforeDelayElapses() {
  AlertScheduler scheduler(std::chrono::seconds(10), std::chrono::seconds(30));
  const auto unfocused_since = T(0);
  CHECK(!scheduler.ShouldAlert(/*focused=*/false, unfocused_since, T(5)));
  CHECK(!scheduler.ShouldAlert(/*focused=*/false, unfocused_since, T(9.9)));
}

void AlertsOnceDelayElapses() {
  AlertScheduler scheduler(std::chrono::seconds(10), std::chrono::seconds(30));
  const auto unfocused_since = T(0);
  CHECK(scheduler.ShouldAlert(/*focused=*/false, unfocused_since, T(10)));
  // Immediately re-checking the same instant does not fire a second time.
  CHECK(!scheduler.ShouldAlert(/*focused=*/false, unfocused_since, T(10)));
}

void RepeatsAtRepeatIntervalWhileStillUnfocused() {
  AlertScheduler scheduler(std::chrono::seconds(10), std::chrono::seconds(30));
  const auto unfocused_since = T(0);
  CHECK(scheduler.ShouldAlert(/*focused=*/false, unfocused_since, T(10)));
  CHECK(!scheduler.ShouldAlert(/*focused=*/false, unfocused_since, T(20)));
  CHECK(!scheduler.ShouldAlert(/*focused=*/false, unfocused_since, T(39.9)));
  CHECK(scheduler.ShouldAlert(/*focused=*/false, unfocused_since, T(40)));
  CHECK(!scheduler.ShouldAlert(/*focused=*/false, unfocused_since, T(69.9)));
  CHECK(scheduler.ShouldAlert(/*focused=*/false, unfocused_since, T(70)));
}

// Becoming focused again has to reset the scheduler, or regaining and
// re-losing focus quickly would skip straight to the repeat cadence
// instead of waiting out the initial delay again.
void BecomingFocusedResetsTheDelay() {
  AlertScheduler scheduler(std::chrono::seconds(10), std::chrono::seconds(30));
  CHECK(scheduler.ShouldAlert(/*focused=*/false, T(0), T(10)));
  CHECK(!scheduler.ShouldAlert(/*focused=*/true, T(0), T(15)));  // focus regained

  // A fresh not-focused streak starting at T(20) must wait out the full
  // delay again before alerting -- not immediately, and not using the old
  // unfocused_since.
  const auto new_unfocused_since = T(20);
  CHECK(!scheduler.ShouldAlert(/*focused=*/false, new_unfocused_since, T(25)));
  CHECK(scheduler.ShouldAlert(/*focused=*/false, new_unfocused_since, T(30)));
}

}  // namespace

int main() {
  RUN_TEST(NeverAlertsWhileFocused);
  RUN_TEST(DoesNotAlertBeforeDelayElapses);
  RUN_TEST(AlertsOnceDelayElapses);
  RUN_TEST(RepeatsAtRepeatIntervalWhileStillUnfocused);
  RUN_TEST(BecomingFocusedResetsTheDelay);
  return test_framework::FailureCount() == 0 ? 0 : 1;
}
