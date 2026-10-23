#include "../src/config.h"
#include "test_framework.h"

namespace {

void DefaultsWhenNoArgsGiven() {
  const auto result = ParseArgs({});
  CHECK(result.ok);
  CHECK(result.config.cascade_path.empty());
  CHECK(result.config.camera_index == 0);
  CHECK_NEAR(result.config.grace_period_seconds, 2.0, 1e-9);
  CHECK_NEAR(result.config.alert_delay_seconds, 10.0, 1e-9);
  CHECK_NEAR(result.config.alert_interval_seconds, 30.0, 1e-9);
  CHECK(result.config.session_log_path == "focus_sessions.csv");
}

void SessionLogFlagOverridesTheDefaultPath() {
  const auto result = ParseArgs({"--session-log", "/tmp/my_sessions.csv"});
  CHECK(result.ok);
  CHECK(result.config.session_log_path == "/tmp/my_sessions.csv");
}

void NoSessionLogFlagClearsThePath() {
  const auto result = ParseArgs({"--no-session-log"});
  CHECK(result.ok);
  CHECK(result.config.session_log_path.empty());
}

void SessionLogFlagMissingItsValueIsRejected() {
  const auto result = ParseArgs({"--session-log"});
  CHECK(!result.ok);
  CHECK(result.error.find("--session-log") != std::string::npos);
}

// The original "./focus_monitor [cascade_path]" positional-argument usage
// has to keep working exactly as before.
void BarePositionalArgumentIsTheCascadePath() {
  const auto result = ParseArgs({"/some/path/to/cascade.xml"});
  CHECK(result.ok);
  CHECK(result.config.cascade_path == "/some/path/to/cascade.xml");
}

void CameraFlagOverridesTheDefault() {
  const auto result = ParseArgs({"--camera", "2"});
  CHECK(result.ok);
  CHECK(result.config.camera_index == 2);
}

void TimingFlagsOverrideTheirDefaults() {
  const auto result =
      ParseArgs({"--grace-period", "5.5", "--alert-delay", "3", "--alert-interval", "15"});
  CHECK(result.ok);
  CHECK_NEAR(result.config.grace_period_seconds, 5.5, 1e-9);
  CHECK_NEAR(result.config.alert_delay_seconds, 3.0, 1e-9);
  CHECK_NEAR(result.config.alert_interval_seconds, 15.0, 1e-9);
}

// Flags and the one positional argument can appear in any order relative
// to each other.
void PositionalArgumentAndFlagsCanBeMixed() {
  const auto result = ParseArgs({"--camera", "1", "cascade.xml", "--grace-period", "1.5"});
  CHECK(result.ok);
  CHECK(result.config.cascade_path == "cascade.xml");
  CHECK(result.config.camera_index == 1);
  CHECK_NEAR(result.config.grace_period_seconds, 1.5, 1e-9);
}

void UnknownFlagIsRejected() {
  const auto result = ParseArgs({"--not-a-real-flag"});
  CHECK(!result.ok);
  CHECK(result.error.find("--not-a-real-flag") != std::string::npos);
}

void FlagMissingItsValueIsRejected() {
  const auto result = ParseArgs({"--camera"});
  CHECK(!result.ok);
  CHECK(result.error.find("--camera") != std::string::npos);
}

void NonNumericCameraValueIsRejected() {
  const auto result = ParseArgs({"--camera", "not-a-number"});
  CHECK(!result.ok);
}

void NumericValueWithTrailingGarbageIsRejected() {
  // std::stoi/std::stod would otherwise silently parse the numeric prefix
  // of "2abc" as 2 and ignore the rest -- that's exactly the kind of typo
  // (a stray character after a number) that should be a hard error, not a
  // silently-accepted camera index.
  const auto result = ParseArgs({"--camera", "2abc"});
  CHECK(!result.ok);
}

void SecondPositionalArgumentIsRejected() {
  const auto result = ParseArgs({"first.xml", "second.xml"});
  CHECK(!result.ok);
  CHECK(result.error.find("second.xml") != std::string::npos);
}

// std::stod parses "nan"/"inf"/"-inf" as a clean, no-exception success --
// none of them mean anything as a duration, and letting one through would
// eventually hit a std::chrono::duration_cast from a double that can't be
// represented as an integer tick count (undefined behavior), not just
// produce a weird but harmless number.
void NonFiniteTimingValueIsRejected() {
  CHECK(!ParseArgs({"--grace-period", "nan"}).ok);
  CHECK(!ParseArgs({"--alert-delay", "inf"}).ok);
  CHECK(!ParseArgs({"--alert-interval", "-inf"}).ok);
}

// A negative duration is just as meaningless as a non-finite one, even
// though std::stod parses "-5" without complaint.
void NegativeTimingValueIsRejected() {
  CHECK(!ParseArgs({"--grace-period", "-1"}).ok);
  CHECK(!ParseArgs({"--alert-delay", "-0.5"}).ok);
  CHECK(!ParseArgs({"--alert-interval", "-30"}).ok);
}

// Zero is a legitimate choice for any of the three (e.g. alert
// immediately, with no grace period at all) -- the boundary itself must
// stay accepted, only values strictly below it should be rejected.
void ZeroTimingValueIsAccepted() {
  const auto result =
      ParseArgs({"--grace-period", "0", "--alert-delay", "0", "--alert-interval", "0"});
  CHECK(result.ok);
  CHECK_NEAR(result.config.grace_period_seconds, 0.0, 1e-9);
  CHECK_NEAR(result.config.alert_delay_seconds, 0.0, 1e-9);
  CHECK_NEAR(result.config.alert_interval_seconds, 0.0, 1e-9);
}

}  // namespace

int main() {
  RUN_TEST(DefaultsWhenNoArgsGiven);
  RUN_TEST(SessionLogFlagOverridesTheDefaultPath);
  RUN_TEST(NoSessionLogFlagClearsThePath);
  RUN_TEST(SessionLogFlagMissingItsValueIsRejected);
  RUN_TEST(BarePositionalArgumentIsTheCascadePath);
  RUN_TEST(CameraFlagOverridesTheDefault);
  RUN_TEST(TimingFlagsOverrideTheirDefaults);
  RUN_TEST(PositionalArgumentAndFlagsCanBeMixed);
  RUN_TEST(UnknownFlagIsRejected);
  RUN_TEST(FlagMissingItsValueIsRejected);
  RUN_TEST(NonNumericCameraValueIsRejected);
  RUN_TEST(NumericValueWithTrailingGarbageIsRejected);
  RUN_TEST(SecondPositionalArgumentIsRejected);
  RUN_TEST(NonFiniteTimingValueIsRejected);
  RUN_TEST(NegativeTimingValueIsRejected);
  RUN_TEST(ZeroTimingValueIsAccepted);
  return test_framework::FailureCount() == 0 ? 0 : 1;
}
