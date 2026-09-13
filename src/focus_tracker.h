#ifndef FOCUS_MONITOR_FOCUS_TRACKER_H_
#define FOCUS_MONITOR_FOCUS_TRACKER_H_

#include <chrono>

// Tracks whether a face has been present in front of the camera recently and
// accumulates how long the session has spent focused vs. not focused.
//
// This is presence-based only: it does not estimate gaze direction or head
// pose, so a face that is visible but not actually looking at the screen
// still counts as "focused". A short grace period absorbs brief detection
// misses (blinks, momentary head turns) so the status does not flicker.
class FocusTracker {
 public:
  using Clock = std::chrono::steady_clock;

  explicit FocusTracker(Clock::time_point now,
                         Clock::duration grace_period = std::chrono::seconds(2))
      : grace_period_(grace_period),
        last_face_seen_(now),
        state_changed_at_(now),
        previous_tick_(now) {}

  // Call once per frame with whether a face was detected in that frame.
  // Returns true if the focus state changed as a result of this update.
  bool Update(bool face_seen, Clock::time_point now) {
    focused_seconds_ += std::chrono::duration<double>(now - previous_tick_).count() *
                         (focused_ ? 1.0 : 0.0);
    unfocused_seconds_ += std::chrono::duration<double>(now - previous_tick_).count() *
                           (focused_ ? 0.0 : 1.0);
    previous_tick_ = now;

    if (face_seen) {
      last_face_seen_ = now;
      if (!focused_) {
        focused_ = true;
        state_changed_at_ = now;
        return true;
      }
      return false;
    }

    if (focused_ && now - last_face_seen_ >= grace_period_) {
      focused_ = false;
      state_changed_at_ = now;
      return true;
    }
    return false;
  }

  bool focused() const { return focused_; }

  double SecondsInCurrentState(Clock::time_point now) const {
    return std::chrono::duration<double>(now - state_changed_at_).count();
  }

  // Fraction of elapsed session time spent focused, in [0, 1].
  double FocusRatio() const {
    const double total = focused_seconds_ + unfocused_seconds_;
    return total > 0.0 ? focused_seconds_ / total : 1.0;
  }

 private:
  Clock::duration grace_period_;
  Clock::time_point last_face_seen_;
  Clock::time_point state_changed_at_;
  Clock::time_point previous_tick_;
  bool focused_ = true;
  double focused_seconds_ = 0.0;
  double unfocused_seconds_ = 0.0;
};

#endif  // FOCUS_MONITOR_FOCUS_TRACKER_H_
