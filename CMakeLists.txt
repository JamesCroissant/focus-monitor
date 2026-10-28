cmake_minimum_required(VERSION 3.16)
project(focus_monitor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(OpenCV REQUIRED)

add_executable(focus_monitor src/main.cpp)
target_include_directories(focus_monitor PRIVATE ${OpenCV_INCLUDE_DIRS})
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
