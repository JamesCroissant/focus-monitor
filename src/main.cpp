#include <opencv2/opencv.hpp>
#include <opencv2/core/utils/filesystem.hpp>
#include <opencv2/objdetect.hpp>

#include <array>
#include <iostream>
#include <string>

namespace {

constexpr std::array<const char*, 4> kCascadeSearchPaths = {
    "data/haarcascades/haarcascade_frontalface_default.xml",
    "/usr/share/opencv4/haarcascades/haarcascade_frontalface_default.xml",
    "/usr/local/share/opencv4/haarcascades/haarcascade_frontalface_default.xml",
    "/usr/share/opencv/haarcascades/haarcascade_frontalface_default.xml",
};

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

    const bool face_found = !faces.empty();
    const std::string status =
        face_found ? "Face detected (" + std::to_string(faces.size()) + ")" : "No face detected";
    cv::putText(frame, status, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.8,
                face_found ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255), 2);

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
