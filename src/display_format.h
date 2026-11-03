#ifndef FOCUS_MONITOR_DISPLAY_FORMAT_H_
#define FOCUS_MONITOR_DISPLAY_FORMAT_H_

#include <sstream>
#include <string>

// Formats the two lines main.cpp draws onto each frame via cv::putText.
// Pulled out as pure string-in/string-out functions -- no OpenCV, no
// camera -- since main.cpp itself has no test coverage at all (it can't
// be exercised without a real camera), and this is the one piece of
// display logic in it that's actually worth getting right independent
// of that: a wrong second count or a mis-scaled percentage would be a
// real, if minor, user-visible defect, and previously had no way to be
// caught except by eye.

// "Focused (12s)" / "Not focused (3s)" -- the on-screen status overlay's
// first line.
inline std::string FormatStatusLine(bool focused, int seconds_in_state) {
  std::ostringstream out;
  out << (focused ? "Focused" : "Not focused") << " (" << seconds_in_state << "s)";
  return out.str();
}

// "Focus rate: 87%" -- the second line. focus_ratio is in [0, 1];
// truncates to the nearest whole percent below the true value (87.6%
// reads as "87%", not rounded up to "88%"), matching a common
// progress-display convention of never showing a round number the
// session hasn't actually reached yet.
inline std::string FormatRatioLine(double focus_ratio) {
  std::ostringstream out;
  out << "Focus rate: " << static_cast<int>(focus_ratio * 100) << "%";
  return out.str();
}

#endif  // FOCUS_MONITOR_DISPLAY_FORMAT_H_
