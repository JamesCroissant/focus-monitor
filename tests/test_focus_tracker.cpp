#include "../src/focus_tracker.h"
#include "test_framework.h"

namespace {

using Clock = FocusTracker::Clock;

Clock::time_point T(double seconds) {
  return Clock::time_point{} + std::chrono::duration_cast<Clock::duration>(
                                   std::chrono::duration<double>(seconds));
}

void StartsFocused() {
  FocusTracker tracker(T(0));
  CHECK(tracker.focused());
}

void StaysFocusedWhileFaceIsSeen() {
  FocusTracker tracker(T(0));
  CHECK(!tracker.Update(/*face_seen=*/true, T(1)));
  CHECK(tracker.focused());
  CHECK(!tracker.Update(/*face_seen=*/true, T(2)));
  CHECK(tracker.focused());
}

// The whole point of the grace period: a face briefly dropping out of
// detection (a blink, a momentary head turn) must not register as losing
// focus, as long as it reappears before the grace period elapses.
void ShortGapWithinGracePeriodDoesNotLoseFocus() {
  FocusTracker tracker(T(0), std::chrono::seconds(2));
  CHECK(!tracker.Update(/*face_seen=*/true, T(1)));
  CHECK(!tracker.Update(/*face_seen=*/false, T(1.5)));  // 0.5s gap, still focused
  CHECK(tracker.focused());
  CHECK(!tracker.Update(/*face_seen=*/true, T(1.8)));  // face back before grace expires
  CHECK(tracker.focused());
}

void LosesFocusOnceGracePeriodElapses() {
  FocusTracker tracker(T(0), std::chrono::seconds(2));
  CHECK(!tracker.Update(/*face_seen=*/true, T(1)));
  CHECK(!tracker.Update(/*face_seen=*/false, T(2)));    // 1s gap: still within grace
  CHECK(tracker.focused());
  CHECK(tracker.Update(/*face_seen=*/false, T(3.1)));   // 2.1s gap: grace period exceeded
  CHECK(!tracker.focused());
  // The transition itself only reports true once, not on every subsequent
  // frame the state stays the same.
  CHECK(!tracker.Update(/*face_seen=*/false, T(4)));
  CHECK(!tracker.focused());
}

void RegainsFocusAsSoonAsFaceReappears() {
  FocusTracker tracker(T(0), std::chrono::seconds(2));
  tracker.Update(/*face_seen=*/true, T(1));
  tracker.Update(/*face_seen=*/false, T(4));  // now unfocused
  CHECK(!tracker.focused());
  CHECK(tracker.Update(/*face_seen=*/true, T(5)));  // immediate regain, no grace on the way back
  CHECK(tracker.focused());
}

void SecondsInCurrentStateTracksTimeSinceTransition() {
  FocusTracker tracker(T(0), std::chrono::seconds(2));
  tracker.Update(/*face_seen=*/true, T(1));
  tracker.Update(/*face_seen=*/false, T(4));  // transitions to unfocused at T(4)
  CHECK_NEAR(tracker.SecondsInCurrentState(T(4)), 0.0, 1e-6);
  CHECK_NEAR(tracker.SecondsInCurrentState(T(9)), 5.0, 1e-6);
}

void FocusRatioReflectsTimeSpentInEachState() {
  FocusTracker tracker(T(0), std::chrono::seconds(2));
  // Each Update() call retroactively credits the *whole* interval since the
  // previous call to whichever state was active before this call -- so the
  // interval in which a transition is detected still counts as the old
  // state, and only the *next* call starts crediting the new one.
  tracker.Update(/*face_seen=*/true, T(10));   // [0, 10) focused: +10 focused
  tracker.Update(/*face_seen=*/false, T(22));  // [10, 22) still credited
                                                // focused (transition detected
                                                // *at* T(22)): +12 focused
  tracker.Update(/*face_seen=*/false, T(32));  // [22, 32) now credited
                                                // unfocused: +10 unfocused
  CHECK_NEAR(tracker.FocusRatio(), 22.0 / 32.0, 1e-6);
}

}  // namespace

int main() {
  RUN_TEST(StartsFocused);
  RUN_TEST(StaysFocusedWhileFaceIsSeen);
  RUN_TEST(ShortGapWithinGracePeriodDoesNotLoseFocus);
  RUN_TEST(LosesFocusOnceGracePeriodElapses);
  RUN_TEST(RegainsFocusAsSoonAsFaceReappears);
  RUN_TEST(SecondsInCurrentStateTracksTimeSinceTransition);
  RUN_TEST(FocusRatioReflectsTimeSpentInEachState);
  return test_framework::FailureCount() == 0 ? 0 : 1;
}
