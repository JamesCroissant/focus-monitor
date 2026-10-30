#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "../src/session_logger.h"
#include "test_framework.h"

namespace {

SessionSummary MakeSummary() {
  // 2024-01-02T03:04:05Z, chosen as a fixed, non-round value so the test
  // actually exercises FormatTimestamp's zero-padding instead of
  // coincidentally passing on an all-zero time.
  SessionSummary summary;
  summary.ended_at = std::chrono::system_clock::from_time_t(1704164645);
  summary.duration_seconds = 123.45;
  summary.focus_ratio = 0.876;
  return summary;
}

void FormatsATimestampDurationAndPercentage() {
  const std::string line = FormatSessionSummaryLine(MakeSummary());
  // 123.45 rounds to 123.5 at one decimal place (fixed/setprecision
  // rounds to nearest, and 123.45 isn't exactly representable in binary
  // floating point -- its actual stored value is just above 123.45).
  CHECK(line == "2024-01-02T03:04:05Z,123.5,87.6");
}

void ZeroFocusRatioFormatsAsZeroPercent() {
  SessionSummary summary = MakeSummary();
  summary.focus_ratio = 0.0;
  const std::string line = FormatSessionSummaryLine(summary);
  CHECK(line.find(",0.0") != std::string::npos);
}

void FullFocusRatioFormatsAsHundredPercent() {
  SessionSummary summary = MakeSummary();
  summary.focus_ratio = 1.0;
  const std::string line = FormatSessionSummaryLine(summary);
  CHECK(line.find(",100.0") != std::string::npos);
}

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream in(path);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

void AppendingToANewFileWritesHeaderThenOneLine() {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "focus_monitor_test_session_log_new.csv";
  std::filesystem::remove(path);

  const bool ok = AppendSessionSummary(path.string(), MakeSummary());
  CHECK(ok);

  const std::string contents = ReadFile(path);
  CHECK(contents == "ended_at,duration_seconds,focus_percent\n2024-01-02T03:04:05Z,123.5,87.6\n");

  std::filesystem::remove(path);
}

void AppendingToAnExistingFileDoesNotDuplicateTheHeader() {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "focus_monitor_test_session_log_existing.csv";
  std::filesystem::remove(path);

  CHECK(AppendSessionSummary(path.string(), MakeSummary()));
  SessionSummary second = MakeSummary();
  second.duration_seconds = 10.0;
  second.focus_ratio = 0.5;
  CHECK(AppendSessionSummary(path.string(), second));

  const std::string contents = ReadFile(path);
  // Exactly one header line, followed by exactly the two data lines.
  CHECK(contents == "ended_at,duration_seconds,focus_percent\n"
                     "2024-01-02T03:04:05Z,123.5,87.6\n"
                     "2024-01-02T03:04:05Z,10.0,50.0\n");

  std::filesystem::remove(path);
}

void WritingToAnUnwritableDirectoryFailsWithoutThrowing() {
  const bool ok =
      AppendSessionSummary("/nonexistent_dir_for_focus_monitor_tests/sessions.csv", MakeSummary());
  CHECK(!ok);
}

void ReadingStatsFromAMissingFileReturnsNullopt() {
  const auto stats = ReadSessionLogStats("/nonexistent_dir_for_focus_monitor_tests/sessions.csv");
  CHECK(!stats.has_value());
}

void ReadingStatsFromAHeaderOnlyFileReturnsZeroSessions() {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "focus_monitor_test_stats_header_only.csv";
  std::ofstream(path) << "ended_at,duration_seconds,focus_percent\n";

  const auto stats = ReadSessionLogStats(path.string());
  CHECK(stats.has_value());
  CHECK(stats->session_count == 0);
  CHECK_NEAR(stats->average_focus_percent, 0.0, 1e-9);

  std::filesystem::remove(path);
}

// The average is weighted by duration, not a plain mean of the two
// percentages -- a 10s@0% session shouldn't cancel out a 90s@100% one
// down to 50%, it should barely move the average at all.
void StatsAverageFocusPercentIsDurationWeighted() {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "focus_monitor_test_stats_weighted.csv";
  std::filesystem::remove(path);
  SessionSummary a = MakeSummary();
  a.duration_seconds = 90.0;
  a.focus_ratio = 1.0;
  SessionSummary b = MakeSummary();
  b.duration_seconds = 10.0;
  b.focus_ratio = 0.0;
  CHECK(AppendSessionSummary(path.string(), a));
  CHECK(AppendSessionSummary(path.string(), b));

  const auto stats = ReadSessionLogStats(path.string());
  CHECK(stats.has_value());
  CHECK(stats->session_count == 2);
  CHECK_NEAR(stats->total_duration_seconds, 100.0, 1e-9);
  CHECK_NEAR(stats->average_focus_percent, 90.0, 1e-9);

  std::filesystem::remove(path);
}

// All sessions on record have zero duration -- nothing to weight by, so
// the average falls back to 0 instead of a division by zero.
void StatsWithOnlyZeroDurationSessionsAveragesToZero() {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "focus_monitor_test_stats_zero_duration.csv";
  std::filesystem::remove(path);
  SessionSummary zero = MakeSummary();
  zero.duration_seconds = 0.0;
  zero.focus_ratio = 1.0;
  CHECK(AppendSessionSummary(path.string(), zero));

  const auto stats = ReadSessionLogStats(path.string());
  CHECK(stats.has_value());
  CHECK(stats->session_count == 1);
  CHECK_NEAR(stats->average_focus_percent, 0.0, 1e-9);

  std::filesystem::remove(path);
}

// A malformed line (truncated write, hand-edited file) is skipped rather
// than aborting the whole read or throwing.
void MalformedLineIsSkippedRatherThanAborting() {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "focus_monitor_test_stats_malformed.csv";
  std::ofstream out(path);
  out << "ended_at,duration_seconds,focus_percent\n"
      << "2024-01-02T03:04:05Z,not-a-number,87.6\n"
      << "2024-01-02T03:04:05Z,10.0,50.0\n";
  out.close();

  const auto stats = ReadSessionLogStats(path.string());
  CHECK(stats.has_value());
  CHECK(stats->session_count == 1);
  CHECK_NEAR(stats->total_duration_seconds, 10.0, 1e-9);

  std::filesystem::remove(path);
}

}  // namespace

int main() {
  RUN_TEST(FormatsATimestampDurationAndPercentage);
  RUN_TEST(ZeroFocusRatioFormatsAsZeroPercent);
  RUN_TEST(FullFocusRatioFormatsAsHundredPercent);
  RUN_TEST(AppendingToANewFileWritesHeaderThenOneLine);
  RUN_TEST(AppendingToAnExistingFileDoesNotDuplicateTheHeader);
  RUN_TEST(WritingToAnUnwritableDirectoryFailsWithoutThrowing);
  RUN_TEST(ReadingStatsFromAMissingFileReturnsNullopt);
  RUN_TEST(ReadingStatsFromAHeaderOnlyFileReturnsZeroSessions);
  RUN_TEST(StatsAverageFocusPercentIsDurationWeighted);
  RUN_TEST(StatsWithOnlyZeroDurationSessionsAveragesToZero);
  RUN_TEST(MalformedLineIsSkippedRatherThanAborting);
  return test_framework::FailureCount() == 0 ? 0 : 1;
}
