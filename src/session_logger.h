#ifndef FOCUS_MONITOR_SESSION_LOGGER_H_
#define FOCUS_MONITOR_SESSION_LOGGER_H_

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

// Records a one-line CSV summary of each session (how long it ran and what
// fraction of it was spent focused) to a log file on exit, so focus trends
// can be reviewed across runs instead of only ever seeing the live
// session's numbers on screen right before the window closes.
struct SessionSummary {
  std::chrono::system_clock::time_point ended_at;
  double duration_seconds = 0.0;
  // Fraction of the session spent focused, in [0, 1] (FocusTracker's
  // FocusRatio()).
  double focus_ratio = 0.0;
};

namespace session_logger_internal {

inline std::string FormatTimestamp(std::chrono::system_clock::time_point tp) {
  const std::time_t time = std::chrono::system_clock::to_time_t(tp);
  std::tm tm_buf{};
  gmtime_r(&time, &tm_buf);
  std::ostringstream out;
  out << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
  return out.str();
}

}  // namespace session_logger_internal

// One CSV line (no trailing newline): ISO-8601 UTC end timestamp, session
// duration in seconds (1 decimal), and focus ratio as a 0-100 percentage
// (1 decimal).
inline std::string FormatSessionSummaryLine(const SessionSummary& summary) {
  std::ostringstream out;
  out << session_logger_internal::FormatTimestamp(summary.ended_at) << "," << std::fixed
      << std::setprecision(1) << summary.duration_seconds << "," << std::fixed
      << std::setprecision(1) << (summary.focus_ratio * 100.0);
  return out.str();
}

// Appends one line (FormatSessionSummaryLine(summary) + "\n") to `path`,
// writing a CSV header first if the file doesn't exist yet (so a fresh
// log is self-describing, and an existing one never gets a duplicate
// header). Best-effort: a session's own tracking must never crash because
// logging couldn't open a file (e.g. the containing directory doesn't
// exist), so this returns false instead of throwing.
inline bool AppendSessionSummary(const std::string& path, const SessionSummary& summary) {
  const bool file_exists = [&] {
    std::ifstream probe(path);
    return probe.good();
  }();

  std::ofstream out(path, std::ios::app);
  if (!out) {
    return false;
  }
  if (!file_exists) {
    out << "ended_at,duration_seconds,focus_percent\n";
  }
  out << FormatSessionSummaryLine(summary) << "\n";
  return static_cast<bool>(out);
}

#endif  // FOCUS_MONITOR_SESSION_LOGGER_H_
