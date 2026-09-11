#include <opencv2/opencv.hpp>

#include <iostream>

int main() {
  cv::VideoCapture camera(0);
  if (!camera.isOpened()) {
    std::cerr << "Failed to open camera 0" << std::endl;
    return 1;
  }

  const std::string window_name = "Focus Monitor";
  cv::namedWindow(window_name, cv::WINDOW_AUTOSIZE);

  cv::Mat frame;
  while (true) {
    camera >> frame;
    if (frame.empty()) {
      std::cerr << "Empty frame received from camera" << std::endl;
      break;
    }

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
