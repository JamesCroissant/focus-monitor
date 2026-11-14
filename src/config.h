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
  // Requested camera resolution (--width/--height), passed straight
  // through to cv::VideoCapture::set(CAP_PROP_FRAME_WIDTH/HEIGHT) in
  // main.cpp. 0 means "don't ask" -- leave the camera at whatever
  // resolution it opens with by default, rather than forcing a
  // potentially-unsupported mode on every user who doesn't care.
  // min_face_size_px's own comment already names resolution as the
  // reason the right face-size value varies; this is what was missing
  // to actually act on that instead of just being stuck with the
  // camera's default.
  int camera_width_px = 0;
  int camera_height_px = 0;
  // --stats: print a summary of session_log_path's history and exit,
  // instead of opening the camera at all (see session_logger.h's
  // ReadSessionLogStats).
  bool show_stats = false;
};

struct ParseArgsResult {
  bool ok = true;
  // Set when the caller passed --help/-h. Takes priority over every
  // other argument -- main.cpp checks this before looking at `ok` or
  // `config` at all, the same way any other command-line tool treats
  // --help as an immediate, always-available request rather than one
  // more flag that has to coexist with whatever else was typed.
  bool help_requested = false;
  std::string error;
  Config config;
};

// Upper bound for --grace-period/--alert-delay/--alert-interval, in
// seconds -- see its one use below for why this exists at all. ~31.7
// years: absurdly larger than any value a real config would ever need,
// but still comfortably inside the range a nanosecond-resolution,
// int64_t-tick-count std::chrono::duration can represent (~292 years),
// which is the actual property this has to hold.
constexpr double kMaxTimingSeconds = 1e9;

// Parses argv[1..] (pass argv+1, argc-1 worth of arguments, not argv[0]).
//
// Accepts at most one bare positional argument (the cascade path, matching
// the original "./focus_monitor [cascade_path]" usage) alongside
// `--camera N`, `--grace-period SECONDS`, `--alert-delay SECONDS`,
// `--alert-interval SECONDS`, `--session-log PATH`, `--no-session-log`,
// `--min-face-size PX`, `--width PX`, `--height PX`, and `--stats`. On any error (unknown flag, missing value, a
// value that doesn't parse as a number, a timing value that isn't finite,
// negative, or too large (see kMaxTimingSeconds), a non-positive face
// size, more than one positional argument), returns `ok == false` with a
// human-readable `error` and an unspecified `config`.
//
// --help/-h anywhere in `args` short-circuits all of the above: returns
// immediately with `help_requested == true` and `ok == true`, without
// validating (or even looking at) anything else -- the same way every
// other command-line tool lets --help win regardless of what else was
// typed alongside it, rather than first complaining about an unrelated
// bad flag or a missing value.
inline ParseArgsResult ParseArgs(const std::vector<std::string>& args) {
  ParseArgsResult result;

  for (const std::string& arg : args) {
    if (arg == "--help" || arg == "-h") {
      result.help_requested = true;
      return result;
    }
  }

  bool have_positional = false;

  auto next_value = [&](size_t& i, const std::string& flag) -> std::optional<std::string> {
    if (i + 1 >= args.size()) {
      result.ok = false;
      result.error = flag + " requires a value";
      return std::nullopt;
    }
    return args[++i];
  };

  // Shared by --min-face-size/--width/--height: all three feed a
  // dimension (pixels) into OpenCV APIs (cv::Size, CAP_PROP_FRAME_*)
  // where zero or negative is nonsensical, and std::stoi parses "0"/
  // "-30" without complaint, so each has to check this explicitly.
  auto parse_positive_int = [&](const std::string& flag, const std::string& value) -> std::optional<int> {
    try {
      size_t consumed = 0;
      const int parsed = std::stoi(value, &consumed);
      if (consumed != value.size()) {
        throw std::invalid_argument("trailing characters");
      }
      if (parsed <= 0) {
        throw std::invalid_argument("not positive");
      }
      return parsed;
    } catch (const std::exception&) {
      result.ok = false;
      result.error = flag + " expects a positive integer, got '" + value + "'";
      return std::nullopt;
    }
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
          const int index = std::stoi(*value, &consumed);
          if (consumed != value->size()) {
            throw std::invalid_argument("trailing characters");
          }
          // Device indices are 0-based; a negative one is never valid and,
          // unlike an out-of-range-but-non-negative index, isn't even
          // worth leaving for cv::VideoCapture to reject at open time with
          // a generic "failed to open" -- this is the same kind of
          // obviously-nonsensical-at-parse-time value --min-face-size/
          // --width/--height already catch for their own (also
          // necessarily non-negative) pixel counts.
          if (index < 0) {
            throw std::invalid_argument("negative camera index");
          }
          result.config.camera_index = index;
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
          // value that happens to work out." A huge-but-finite value
          // hits the exact same undefined behavior for the exact same
          // reason: main.cpp's Clock::duration is nanosecond-resolution
          // with an int64_t tick count, so anything past roughly 2^63
          // nanoseconds (~292 years) overflows that cast too --
          // reproduced directly with -fsanitize=float-cast-overflow
          // flagging "1e+29 is outside the range of representable
          // values of type 'long int'" for duration_cast's internal
          // double-to-tick-count conversion. kMaxTimingSeconds is
          // nowhere near that boundary (it only has to be larger than
          // any value a real config would ever use), just comfortably
          // on the safe side of it.
          if (!std::isfinite(seconds) || seconds < 0.0 || seconds > kMaxTimingSeconds) {
            throw std::invalid_argument("not a finite number in a sane range");
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
      const auto px = parse_positive_int(arg, *value);
      if (!px) {
        return result;
      }
      result.config.min_face_size_px = *px;
    } else if (arg == "--width" || arg == "--height") {
      const auto value = next_value(i, arg);
      if (!value) {
        return result;
      }
      const auto px = parse_positive_int(arg, *value);
      if (!px) {
        return result;
      }
      (arg == "--width" ? result.config.camera_width_px : result.config.camera_height_px) = *px;
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
  // empty path with a confusing "no log found at ''" error. An empty
  // path isn't only reachable via --no-session-log: `--session-log ""`
  // gets there too, so the message names the actual condition (an
  // empty path) rather than pointing at one specific way to cause it.
  if (result.config.show_stats && result.config.session_log_path.empty()) {
    result.ok = false;
    result.error =
        "--stats requires a non-empty session log path; it can't be combined with "
        "--no-session-log or an empty --session-log value";
    return result;
  }

  return result;
}

#endif  // FOCUS_MONITOR_CONFIG_H_
