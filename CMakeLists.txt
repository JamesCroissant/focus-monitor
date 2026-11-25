cmake_minimum_required(VERSION 3.16)
project(focus_monitor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Unlike os-practice (this account's other from-scratch project, whose
# Makefile has carried -Wall -Wextra since its very first commit), this
# project had no compiler warnings enabled at all -- not even -Wall --
# despite every one of its own commit messages already treating "no new
# compiler warning" as part of what a change has to verify. Applied
# globally (before any add_executable below) so focus_monitor and every
# test executable get it, not just one target. Confirmed the existing
# codebase (main.cpp, every src/*.h, every tests/*.cpp) already builds
# clean under it -- this is a safety net for future changes, not a fix
# for a warning that already existed.
#
# -Werror turns that safety net into an actual gate: -Wall/-Wextra alone
# only ever get looked at if someone happens to scroll past them in a
# build log, same as before CI's own build step existed to run ctest
# automatically instead of trusting a commit message's own "ran it by
# hand" claim. A future warning should fail the same way a failing test
# already does, not sit there unread.
#
# -Wshadow and -Wpedantic were probed (on top of -Wall/-Wextra) when
# -Werror was added, confirmed clean, and deliberately left out of the
# actual flags at the time -- this adds them for real now that nothing
# stands in the way: -Wshadow catches a local/parameter silently
# shadowing an outer-scope or member variable (a real, recurring class
# of bug elsewhere, not a style nit -- the kind of mistake where the
# code reads as if it's updating one variable while actually creating
# and discarding a second one of the same name), and -Wpedantic flags
# non-standard compiler extensions that would silently stop building
# under a different compiler.
add_compile_options(-Wall -Wextra -Wshadow -Wpedantic -Werror)

find_package(OpenCV REQUIRED)

add_executable(focus_monitor src/main.cpp)
# SYSTEM, not just PRIVATE: -Werror above means ANY warning now fails
# the build, including one that originates inside a header this project
# doesn't own -- without SYSTEM, a future OpenCV upgrade (CI installs
# whatever libopencv-dev the runner's own package index currently has,
# not a pinned version) emitting so much as one new deprecation warning
# from its own public headers would break this build for a reason
# nothing in this repo caused or can fix by itself. SYSTEM tells the
# compiler to treat OpenCV's headers the same way it already treats the
# C++ standard library's own -- suppressing warnings that originate
# inside them regardless of -Wall/-Wextra/-Werror, while still fully
# enforcing all three against this project's own code.
target_include_directories(focus_monitor SYSTEM PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(focus_monitor PRIVATE ${OpenCV_LIBS})

# FocusTracker/AlertScheduler are plain, dependency-free logic classes (no
# OpenCV, no camera, no I/O), so their tests don't need OpenCV or a test
# framework dependency either -- just a tiny custom harness
# (tests/test_framework.h) and CTest to run them.
enable_testing()

add_executable(focus_tracker_test tests/test_focus_tracker.cpp)
add_test(NAME focus_tracker_test COMMAND focus_tracker_test)

add_executable(alert_scheduler_test tests/test_alert_scheduler.cpp)
add_test(NAME alert_scheduler_test COMMAND alert_scheduler_test)

add_executable(config_test tests/test_config.cpp)
add_test(NAME config_test COMMAND config_test)

add_executable(session_logger_test tests/test_session_logger.cpp)
add_test(NAME session_logger_test COMMAND session_logger_test)

add_executable(notifier_test tests/test_notifier.cpp)
add_test(NAME notifier_test COMMAND notifier_test)

add_executable(shutdown_flag_test tests/test_shutdown_flag.cpp)
add_test(NAME shutdown_flag_test COMMAND shutdown_flag_test)

add_executable(cascade_resolver_test tests/test_cascade_resolver.cpp)
add_test(NAME cascade_resolver_test COMMAND cascade_resolver_test)

add_executable(consecutive_failure_guard_test tests/test_consecutive_failure_guard.cpp)
add_test(NAME consecutive_failure_guard_test COMMAND consecutive_failure_guard_test)

add_executable(display_format_test tests/test_display_format.cpp)
add_test(NAME display_format_test COMMAND display_format_test)

add_executable(display_availability_test tests/test_display_availability.cpp)
add_test(NAME display_availability_test COMMAND display_availability_test)
