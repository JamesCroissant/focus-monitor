#ifndef FOCUS_MONITOR_TESTS_TEST_FRAMEWORK_H_
#define FOCUS_MONITOR_TESTS_TEST_FRAMEWORK_H_

// A deliberately tiny test harness instead of pulling in a framework
// dependency (gtest, Catch2, ...) this project otherwise has no need for.
// Each test is a `void()` function that uses CHECK/CHECK_NEAR; RUN_TEST
// records pass/fail and keeps going so one failure doesn't hide the rest.

#include <cmath>
#include <iostream>
#include <string>

namespace test_framework {

inline int& FailureCount() {
  static int count = 0;
  return count;
}

inline void ReportFailure(const char* file, int line, const std::string& message) {
  std::cerr << file << ":" << line << ": FAILED: " << message << std::endl;
  ++FailureCount();
}

}  // namespace test_framework

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      test_framework::ReportFailure(__FILE__, __LINE__, "CHECK(" #condition ")"); \
    }                                                                           \
  } while (false)

#define CHECK_NEAR(actual, expected, tolerance)                                            \
  do {                                                                                      \
    const double actual_value = (actual);                                                   \
    const double expected_value = (expected);                                                \
    if (std::fabs(actual_value - expected_value) > (tolerance)) {                            \
      test_framework::ReportFailure(                                                         \
          __FILE__, __LINE__,                                                                \
          "CHECK_NEAR(" #actual ", " #expected ") : " + std::to_string(actual_value) +        \
              " vs " + std::to_string(expected_value));                                      \
    }                                                                                        \
  } while (false)

#define RUN_TEST(test_fn)                                 \
  do {                                                     \
    std::cout << "[ RUN      ] " #test_fn << std::endl;    \
    test_fn();                                              \
    std::cout << "[ DONE     ] " #test_fn << std::endl;    \
  } while (false)

#endif  // FOCUS_MONITOR_TESTS_TEST_FRAMEWORK_H_
