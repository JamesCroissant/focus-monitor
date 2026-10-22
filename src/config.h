#ifndef FOCUS_MONITOR_CONFIG_H_
#define FOCUS_MONITOR_CONFIG_H_

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
// `--alert-interval SECONDS`, `--session-log PATH`, and
// `--no-session-log`. On any error (unknown flag, missing value, a value
// that doesn't parse as a number, more than one positional argument),
// returns `ok == false` with a human-readable `error` and an unspecified
// `config`.
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

  return result;
}

#endif  // FOCUS_MONITOR_CONFIG_H_
