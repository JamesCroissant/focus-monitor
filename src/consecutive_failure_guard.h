#ifndef FOCUS_MONITOR_CONSECUTIVE_FAILURE_GUARD_H_
#define FOCUS_MONITOR_CONSECUTIVE_FAILURE_GUARD_H_

// Tracks consecutive failures of some operation and reports when a
// caller should give up rather than keep retrying forever. One failed
// attempt is normal and expected to recover on its own -- main.cpp uses
// this around `camera >> frame`, where a single empty frame can happen
// transiently on an otherwise healthy camera (a momentary decode
// hiccup, a buffer underrun) and shouldn't by itself be treated as the
// camera having disconnected. RecordFailure() returning true means
// enough failures have happened in a row, with no RecordSuccess() in
// between, that the condition is almost certainly permanent instead.
class ConsecutiveFailureGuard {
 public:
  explicit ConsecutiveFailureGuard(int max_consecutive_failures)
      : max_consecutive_failures_(max_consecutive_failures) {}

  void RecordSuccess() { count_ = 0; }

  // Returns true once this many consecutive failures have been
  // recorded without an intervening RecordSuccess() call.
  bool RecordFailure() { return ++count_ >= max_consecutive_failures_; }

 private:
  int max_consecutive_failures_;
  int count_ = 0;
};

#endif  // FOCUS_MONITOR_CONSECUTIVE_FAILURE_GUARD_H_
