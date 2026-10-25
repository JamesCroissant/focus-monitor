#ifndef FOCUS_MONITOR_SHUTDOWN_FLAG_H_
#define FOCUS_MONITOR_SHUTDOWN_FLAG_H_

#include <atomic>
#include <csignal>

// Lets main's camera loop exit cleanly on SIGINT/SIGTERM (Ctrl-C, or a
// plain `kill`) instead of the default handler terminating the process
// immediately -- without this, pressing Ctrl-C skips camera.release(),
// cv::destroyAllWindows(), and the session-log write that normally runs
// after the loop, silently losing that session's summary.
//
// A single global flag, not a per-instance object: a process only ever
// has one "has shutdown been requested" answer, and the one place that
// sets it -- a signal handler -- can only call async-signal-safe
// functions. A relaxed atomic store/load is one of the few things
// guaranteed safe to do from inside one.
class ShutdownFlag {
 public:
  static ShutdownFlag& Instance() {
    static ShutdownFlag instance;
    return instance;
  }

  void Request() { requested_.store(true, std::memory_order_relaxed); }
  bool Requested() const { return requested_.load(std::memory_order_relaxed); }

  // For tests only, so one test raising a signal doesn't leave the
  // (process-global) flag set for whichever test runs next.
  void ResetForTesting() { requested_.store(false, std::memory_order_relaxed); }

  ShutdownFlag(const ShutdownFlag&) = delete;
  ShutdownFlag& operator=(const ShutdownFlag&) = delete;

 private:
  ShutdownFlag() = default;
  std::atomic<bool> requested_{false};
};

inline void HandleShutdownSignal(int /*signum*/) { ShutdownFlag::Instance().Request(); }

// Installs HandleShutdownSignal for SIGINT and SIGTERM. Call once from
// main(), before entering the camera loop.
inline void InstallShutdownSignalHandlers() {
  std::signal(SIGINT, HandleShutdownSignal);
  std::signal(SIGTERM, HandleShutdownSignal);
}

#endif  // FOCUS_MONITOR_SHUTDOWN_FLAG_H_
