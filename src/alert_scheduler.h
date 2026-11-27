#ifndef FOCUS_MONITOR_ALERT_SCHEDULER_H_
#define FOCUS_MONITOR_ALERT_SCHEDULER_H_

#include <chrono>

// Decides when an "unfocused" alert should fire, decoupled from how the
// alert is actually delivered (desktop notification, sound, log, ...).
//
// An alert fires once `delay` after the not-focused state began, and then
// repeats every `repeat_interval` for as long as the state stays
// not-focused. Becoming focused again resets the scheduler.
//
// `repeat_interval` of exactly zero is accepted (config.h's own range
// check allows it, the same as `delay`), but it doesn't mean "repeat
// very frequently" the way a small-but-nonzero value would -- the
// `now - last_alert_at_ >= repeat_interval_` check below is trivially
// true for any `now` once `repeat_interval_` is 0, so ShouldAlert()
// returns true on literally every call once `delay` has elapsed, not
// on some bounded cadence. Since main.cpp calls this once per
// processed camera frame, that means firing on every frame (commonly
// ~30/s) for as long as the state stays not-focused, not a "fast
// repeat" -- worth knowing before setting `--alert-interval 0`
// expecting rate-limited nagging rather than continuous alerting.
class AlertScheduler {
 public:
  using Clock = std::chrono::steady_clock;

  AlertScheduler(Clock::duration delay, Clock::duration repeat_interval)
      : delay_(delay), repeat_interval_(repeat_interval) {}

  // Call once per frame. `unfocused_since` is the timestamp at which the
  // current not-focused streak began (ignored while `focused` is true).
  // Returns true exactly on the frames where an alert should be delivered.
  bool ShouldAlert(bool focused, Clock::time_point unfocused_since, Clock::time_point now) {
    if (focused) {
      fired_at_least_once_ = false;
      return false;
    }

    if (now - unfocused_since < delay_) {
      return false;
    }

    if (!fired_at_least_once_ || now - last_alert_at_ >= repeat_interval_) {
      fired_at_least_once_ = true;
      last_alert_at_ = now;
      return true;
    }
    return false;
  }

 private:
  Clock::duration delay_;
  Clock::duration repeat_interval_;
  bool fired_at_least_once_ = false;
  Clock::time_point last_alert_at_{};
};

#endif  // FOCUS_MONITOR_ALERT_SCHEDULER_H_
