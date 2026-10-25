#include <opencv2/opencv.hpp>
#include <opencv2/core/utils/filesystem.hpp>
#include <opencv2/objdetect.hpp>

#include <array>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "alert_scheduler.h"
#include "config.h"
#include "focus_tracker.h"
#include "notifier.h"
#include "session_logger.h"
#include "shutdown_flag.h"

namespace {

constexpr std::array<const char*, 4> kCascadeSearchPaths = {
    "data/haarcascades/haarcascade_frontalface_default.xml",
    "/usr/share/opencv4/haarcascades/haarcascade_frontalface_default.xml",
    "/usr/local/share/opencv4/haarcascades/haarcascade_frontalface_default.xml",
    "/usr/share/opencv/haarcascades/haarcascade_frontalface_default.xml",
};

std::string ResolveCascadePath(const std::string& explicit_path) {
  if (!explicit_path.empty()) {
    return explicit_path;
  }
  for (const char* path : kCascadeSearchPaths) {
    if (cv::utils::fs::exists(path)) {
      return path;
    }
  }
  return kCascadeSearchPaths.front();
}

template <typename DurationT>
DurationT SecondsToDuration(double seconds) {
  return std::chrono::duration_cast<DurationT>(std::chrono::duration<double>(seconds));
}

}  // namespace

int main(int argc, char** argv) {
  const std::vector<std::string> raw_args(argv + 1, argv + argc);
  const ParseArgsResult parsed = ParseArgs(raw_args);
  if (!parsed.ok) {
    std::cerr << "Error: " << parsed.error << std::endl;
    std::cerr << "Usage: " << argv[0]
              << " [cascade_path] [--camera N] [--grace-period SECONDS]"
                 " [--alert-delay SECONDS] [--alert-interval SECONDS]"
                 " [--session-log PATH] [--no-session-log]"
              << std::endl;
    return 1;
  }
  const Config& config = parsed.config;

  const std::string cascade_path = ResolveCascadePath(config.cascade_path);

  cv::CascadeClassifier face_cascade;
  if (!face_cascade.load(cascade_path)) {
    std::cerr << "Failed to load face cascade from: " << cascade_path << std::endl;
    std::cerr << "Pass the path to haarcascade_frontalface_default.xml as an argument."
              << std::endl;
    return 1;
  }

  cv::VideoCapture camera(config.camera_index);
  if (!camera.isOpened()) {
    std::cerr << "Failed to open camera " << config.camera_index << std::endl;
    return 1;
  }

  InstallShutdownSignalHandlers();

  const std::string window_name = "Focus Monitor";
  cv::namedWindow(window_name, cv::WINDOW_AUTOSIZE);

  const auto session_start = FocusTracker::Clock::now();
  FocusTracker tracker(session_start, SecondsToDuration<FocusTracker::Clock::duration>(
                                           config.grace_period_seconds));
  AlertScheduler alert_scheduler(
      SecondsToDuration<AlertScheduler::Clock::duration>(config.alert_delay_seconds),
      SecondsToDuration<AlertScheduler::Clock::duration>(config.alert_interval_seconds));
  Notifier notifier;

  cv::Mat frame;
  cv::Mat gray;
  std::vector<cv::Rect> faces;
  while (!ShutdownFlag::Instance().Requested()) {
    camera >> frame;
    if (frame.empty()) {
      std::cerr << "Empty frame received from camera" << std::endl;
      break;
    }

    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::equalizeHist(gray, gray);

    faces.clear();
    face_cascade.detectMultiScale(gray, faces, 1.1, 4, 0, cv::Size(60, 60));

    for (const auto& face : faces) {
      cv::rectangle(frame, face, cv::Scalar(0, 255, 0), 2);
    }

    const auto now = FocusTracker::Clock::now();
    if (tracker.Update(!faces.empty(), now)) {
      std::cout << "[focus-monitor] "
                << (tracker.focused() ? "Focus regained" : "Focus lost") << std::endl;
    }

    if (alert_scheduler.ShouldAlert(tracker.focused(), tracker.StateChangedAt(), now)) {
      const int unfocused_seconds = static_cast<int>(tracker.SecondsInCurrentState(now));
      notifier.Notify("Focus Monitor", "No face detected for " +
                                            std::to_string(unfocused_seconds) +
                                            "s. Are you still there?");
    }

    std::ostringstream status_line;
    status_line << (tracker.focused() ? "Focused" : "Not focused") << " ("
                << static_cast<int>(tracker.SecondsInCurrentState(now)) << "s)";
    cv::putText(frame, status_line.str(), cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.8,
                tracker.focused() ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255), 2);

    std::ostringstream ratio_line;
    ratio_line << "Focus rate: " << static_cast<int>(tracker.FocusRatio() * 100) << "%";
    cv::putText(frame, ratio_line.str(), cv::Point(10, 60), cv::FONT_HERSHEY_SIMPLEX, 0.7,
                cv::Scalar(255, 255, 255), 2);

    cv::imshow(window_name, frame);

    // Exit on 'q' or ESC.
    int key = cv::waitKey(1) & 0xFF;
    if (key == 'q' || key == 27) {
      break;
    }
  }

  if (ShutdownFlag::Instance().Requested()) {
    std::cout << "[focus-monitor] Shutting down (signal received)" << std::endl;
  }

  camera.release();
  cv::destroyAllWindows();

  if (!config.session_log_path.empty()) {
    const double duration_seconds =
        std::chrono::duration<double>(FocusTracker::Clock::now() - session_start).count();
    const SessionSummary summary{std::chrono::system_clock::now(), duration_seconds,
                                  tracker.FocusRatio()};
    if (!AppendSessionSummary(config.session_log_path, summary)) {
      std::cerr << "[focus-monitor] Warning: failed to write session log to "
                << config.session_log_path << std::endl;
    }
  }

  return 0;
}
