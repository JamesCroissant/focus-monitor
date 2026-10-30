#ifndef FOCUS_MONITOR_SESSION_LOGGER_H_
#define FOCUS_MONITOR_SESSION_LOGGER_H_

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <optional>
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

// Summary of everything AppendSessionSummary has ever written to one log
// file -- the read-back counterpart to it, so a user actually has a way
// to see focus trends across sessions instead of only ever seeing one
// session's own numbers on screen right before the window closes.
struct SessionLogStats {
  int session_count = 0;
  double total_duration_seconds = 0.0;
  // Duration-weighted average focus percentage across all sessions, in
  // [0, 100] -- a 10-minute session at 90% should count for more than a
  // 10-second one at 10%, so this isn't a plain arithmetic mean of the
  // per-session percentages. 0 when there are no sessions yet, or when
  // every session on record happens to have zero duration (nothing to
  // weight by).
  double average_focus_percent = 0.0;
};

// Reads back every line AppendSessionSummary has ever appended to `path`
// and summarizes it. Returns std::nullopt if `path` doesn't exist or
// can't be opened -- distinct from a SessionLogStats with
// session_count == 0, which means the file exists (e.g. just the header,
// or empty) but has no actual session lines yet. A line that doesn't
// parse as "text,number,number" (a truncated write, a hand-edited file)
// is skipped rather than aborting the whole read.
inline std::optional<SessionLogStats> ReadSessionLogStats(const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    return std::nullopt;
  }

  SessionLogStats stats;
  double weighted_focus_sum = 0.0;
  std::string line;
  bool first_line = true;
  while (std::getline(in, line)) {
    const bool is_header = first_line && line == "ended_at,duration_seconds,focus_percent";
    first_line = false;
    if (is_header || line.empty()) {
      continue;
    }

    const auto first_comma = line.find(',');
    const auto second_comma =
        first_comma == std::string::npos ? std::string::npos : line.find(',', first_comma + 1);
    if (first_comma == std::string::npos || second_comma == std::string::npos) {
      continue;
    }
    try {
      const double duration = std::stod(line.substr(first_comma + 1, second_comma - first_comma - 1));
      const double focus_percent = std::stod(line.substr(second_comma + 1));
      ++stats.session_count;
      stats.total_duration_seconds += duration;
      weighted_focus_sum += duration * focus_percent;
    } catch (const std::exception&) {
      continue;
    }
  }

  if (stats.total_duration_seconds > 0.0) {
    stats.average_focus_percent = weighted_focus_sum / stats.total_duration_seconds;
  }
  return stats;
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
