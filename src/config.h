#ifndef FOCUS_MONITOR_CONFIG_H_
#define FOCUS_MONITOR_CONFIG_H_

#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// Everything about a run that used to be a hardcoded constant in main.cpp
// (camera index, grace period, alert delay/interval) or a bare positional
// argument (the cascade path) -- gathered in one place and parsed from the
// command line, with no OpenCV dependency, so the parsing logic itself can
// be unit-tested without a camera or a display.
struct Config {
  // Empty means "auto-detect" (see main.cpp's ResolveCascadePath).
  std::string cascade_path;
  int camera_index = 0;
  double grace_period_seconds = 2.0;
  double alert_delay_seconds = 10.0;
  double alert_interval_seconds = 30.0;
  // Where each session's summary line is appended on exit (see
  // session_logger.h). Empty means "don't log" (--no-session-log).
  std::string session_log_path = "focus_sessions.csv";
  // Smallest face (in pixels, both dimensions) detectMultiScale will
  // report, passed straight through as its minSize argument. The right
  // value depends on the camera's resolution and how far it sits from
  // the user -- a value tuned for a close-up laptop webcam is too large
  // for a desk-mounted camera further away, and vice versa.
  int min_face_size_px = 60;
  // --stats: print a summary of session_log_path's history and exit,
  // instead of opening the camera at all (see session_logger.h's
  // ReadSessionLogStats).
  bool show_stats = false;
};

struct ParseArgsResult {
  bool ok = true;
  std::string error;
  Config config;
};

// Parses argv[1..] (pass argv+1, argc-1 worth of arguments, not argv[0]).
//
// Accepts at most one bare positional argument (the cascade path, matching
// the original "./focus_monitor [cascade_path]" usage) alongside
// `--camera N`, `--grace-period SECONDS`, `--alert-delay SECONDS`,
// `--alert-interval SECONDS`, `--session-log PATH`, `--no-session-log`,
// `--min-face-size PX`, and `--stats`. On any error (unknown flag, missing value, a
// value that doesn't parse as a number, a timing value that isn't finite
// and non-negative, a non-positive face size, more than one positional
// argument), returns `ok == false` with a human-readable `error` and an
// unspecified `config`.
inline ParseArgsResult ParseArgs(const std::vector<std::string>& args) {
  ParseArgsResult result;
  bool have_positional = false;

  auto next_value = [&](size_t& i, const std::string& flag) -> std::optional<std::string> {
    if (i + 1 >= args.size()) {
      result.ok = false;
      result.error = flag + " requires a value";
      return std::nullopt;
    }
    return args[++i];
  };

  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];

    if (arg == "--camera" || arg == "--grace-period" || arg == "--alert-delay" ||
        arg == "--alert-interval") {
      const auto value = next_value(i, arg);
      if (!value) {
        return result;
      }
      try {
        if (arg == "--camera") {
          size_t consumed = 0;
          result.config.camera_index = std::stoi(*value, &consumed);
          if (consumed != value->size()) {
            throw std::invalid_argument("trailing characters");
          }
        } else {
          size_t consumed = 0;
          const double seconds = std::stod(*value, &consumed);
          if (consumed != value->size()) {
            throw std::invalid_argument("trailing characters");
          }
          // std::stod happily parses "nan"/"inf"/"-inf" as a finite-
          // looking success (no exception, full string consumed), and a
          // negative duration is just as meaningless here -- all three
          // would otherwise flow into a std::chrono::duration_cast from
          // a double that can't be represented as a duration's integer
          // tick count, which is undefined behavior, not just "a weird
          // value that happens to work out."
          if (!std::isfinite(seconds) || seconds < 0.0) {
            throw std::invalid_argument("not a finite, non-negative number");
          }
          if (arg == "--grace-period") {
            result.config.grace_period_seconds = seconds;
          } else if (arg == "--alert-delay") {
            result.config.alert_delay_seconds = seconds;
          } else {
            result.config.alert_interval_seconds = seconds;
          }
        }
      } catch (const std::exception&) {
        result.ok = false;
        result.error = arg + " expects a number, got '" + *value + "'";
        return result;
      }
    } else if (arg == "--session-log") {
      const auto value = next_value(i, arg);
      if (!value) {
        return result;
      }
      result.config.session_log_path = *value;
    } else if (arg == "--no-session-log") {
      result.config.session_log_path.clear();
    } else if (arg == "--stats") {
      result.config.show_stats = true;
    } else if (arg == "--min-face-size") {
      const auto value = next_value(i, arg);
      if (!value) {
        return result;
      }
      try {
        size_t consumed = 0;
        const int px = std::stoi(*value, &consumed);
        if (consumed != value->size()) {
          throw std::invalid_argument("trailing characters");
        }
        // A zero or negative minSize isn't just an odd setting --
        // cv::Size's width/height feed directly into OpenCV's cascade
        // scaling math, where a non-positive dimension is nonsensical.
        if (px <= 0) {
          throw std::invalid_argument("not positive");
        }
        result.config.min_face_size_px = px;
      } catch (const std::exception&) {
        result.ok = false;
        result.error = arg + " expects a positive integer, got '" + *value + "'";
        return result;
      }
    } else if (arg.rfind("--", 0) == 0) {
      result.ok = false;
      result.error = "unknown flag: " + arg;
      return result;
    } else {
      if (have_positional) {
        result.ok = false;
        result.error = "unexpected extra argument: " + arg;
        return result;
      }
      result.config.cascade_path = arg;
      have_positional = true;
    }
  }

  // --stats has nothing to read back without a log path -- catching this
  // here, once, instead of leaving ReadSessionLogStats to fail on an
  // empty path with a confusing "no log found at ''" error.
  if (result.config.show_stats && result.config.session_log_path.empty()) {
    result.ok = false;
    result.error = "--stats requires a session log; it can't be combined with --no-session-log";
    return result;
  }

  return result;
}

#endif  // FOCUS_MONITOR_CONFIG_H_
