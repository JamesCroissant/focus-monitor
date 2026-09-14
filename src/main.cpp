#include <opencv2/opencv.hpp>
#include <opencv2/core/utils/filesystem.hpp>
#include <opencv2/objdetect.hpp>

#include <array>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

#include "alert_scheduler.h"
#include "focus_tracker.h"
#include "notifier.h"

namespace {

constexpr std::array<const char*, 4> kCascadeSearchPaths = {
    "data/haarcascades/haarcascade_frontalface_default.xml",
    "/usr/share/opencv4/haarcascades/haarcascade_frontalface_default.xml",
    "/usr/local/share/opencv4/haarcascades/haarcascade_frontalface_default.xml",
    "/usr/share/opencv/haarcascades/haarcascade_frontalface_default.xml",
};

// How long to stay not-focused before the first alert, and how often to
// repeat it while the not-focused streak continues.
constexpr std::chrono::seconds kUnfocusedAlertDelay{10};
constexpr std::chrono::seconds kUnfocusedAlertRepeatInterval{30};

std::string ResolveCascadePath(int argc, char** argv) {
  if (argc > 1) {
    return argv[1];
  }
  for (const char* path : kCascadeSearchPaths) {
    if (cv::utils::fs::exists(path)) {
      return path;
    }
  }
  return kCascadeSearchPaths.front();
}

}  // namespace

int main(int argc, char** argv) {
  const std::string cascade_path = ResolveCascadePath(argc, argv);

  cv::CascadeClassifier face_cascade;
  if (!face_cascade.load(cascade_path)) {
    std::cerr << "Failed to load face cascade from: " << cascade_path << std::endl;
    std::cerr << "Pass the path to haarcascade_frontalface_default.xml as an argument."
              << std::endl;
    return 1;
  }

  cv::VideoCapture camera(0);
  if (!camera.isOpened()) {
    std::cerr << "Failed to open camera 0" << std::endl;
    return 1;
  }

  const std::string window_name = "Focus Monitor";
  cv::namedWindow(window_name, cv::WINDOW_AUTOSIZE);

  FocusTracker tracker(FocusTracker::Clock::now());
  AlertScheduler alert_scheduler(kUnfocusedAlertDelay, kUnfocusedAlertRepeatInterval);
  Notifier notifier;

  cv::Mat frame;
  cv::Mat gray;
  std::vector<cv::Rect> faces;
  while (true) {
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

  camera.release();
  cv::destroyAllWindows();
  return 0;
}
