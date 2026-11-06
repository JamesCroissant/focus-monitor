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

// The grace-period check is `now - last_face_seen_ >= grace_period_`, not
// `>` -- landing exactly on the boundary already counts as exceeded, not
// still-tolerated. None of the other grace-period tests land exactly on
// this edge (they're comfortably inside or past it); AlertScheduler's own
// boundary tests (e.g. AlertsOnceDelayElapses) do test their exact edge,
// so this closes the same gap here.
void LosesFocusExactlyAtGracePeriodBoundary() {
  FocusTracker tracker(T(0), std::chrono::seconds(2));
  CHECK(!tracker.Update(/*face_seen=*/true, T(1)));  // last_face_seen_ = T(1)
  CHECK(tracker.Update(/*face_seen=*/false, T(3)));  // exactly a 2.0s gap
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

// main.cpp "closes out" a session before logging its summary by calling
// Update(focused(), session_end) one last time -- passing the tracker's
// own current state back in as face_seen, so the elapsed-time accounting
// catches up to the real end of the session (not just the last frame
// actually processed) without that call itself being allowed to toggle
// anything. Both directions (closing out while focused, and while
// unfocused) must be true no-ops for the state itself.
void ClosingOutWithTheCurrentStateOnlyAdvancesTimeNeverTogglesState() {
  FocusTracker focused_tracker(T(0), std::chrono::seconds(2));
  CHECK(!focused_tracker.Update(/*face_seen=*/true, T(10)));
  CHECK(!focused_tracker.Update(focused_tracker.focused(), T(20)));  // close-out
  CHECK(focused_tracker.focused());
  CHECK_NEAR(focused_tracker.FocusRatio(), 1.0, 1e-9);

  FocusTracker unfocused_tracker(T(0), std::chrono::seconds(2));
  CHECK(!unfocused_tracker.Update(/*face_seen=*/true, T(1)));
  CHECK(unfocused_tracker.Update(/*face_seen=*/false, T(4)));  // now unfocused
  CHECK(!unfocused_tracker.Update(unfocused_tracker.focused(), T(9)));  // close-out
  CHECK(!unfocused_tracker.focused());
  // [0, 4) credited focused (4s), [4, 9) credited unfocused (5s) --
  // including the close-out's own [4, 9) span, not just [4, 4).
  CHECK_NEAR(unfocused_tracker.FocusRatio(), 4.0 / 9.0, 1e-9);
}

}  // namespace

int main() {
  RUN_TEST(StartsFocused);
  RUN_TEST(StaysFocusedWhileFaceIsSeen);
  RUN_TEST(ShortGapWithinGracePeriodDoesNotLoseFocus);
  RUN_TEST(LosesFocusOnceGracePeriodElapses);
  RUN_TEST(LosesFocusExactlyAtGracePeriodBoundary);
  RUN_TEST(RegainsFocusAsSoonAsFaceReappears);
  RUN_TEST(SecondsInCurrentStateTracksTimeSinceTransition);
  RUN_TEST(FocusRatioReflectsTimeSpentInEachState);
  RUN_TEST(ClosingOutWithTheCurrentStateOnlyAdvancesTimeNeverTogglesState);
  return test_framework::FailureCount() == 0 ? 0 : 1;
}
