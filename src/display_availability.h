#ifndef FOCUS_MONITOR_DISPLAY_AVAILABILITY_H_
#define FOCUS_MONITOR_DISPLAY_AVAILABILITY_H_

// Whether a GUI display session is available for cv::namedWindow()/
// cv::imshow() to actually open a window on. Takes the two relevant
// environment variables' values as plain arguments (rather than calling
// std::getenv() itself) specifically so this decision -- which is pure
// string logic -- can be unit-tested without actually manipulating the
// process environment.
//
// Checking this *before* ever calling cv::namedWindow() matters because
// OpenCV's Qt/GTK highgui backend doesn't fail that call with a
// catchable C++ exception when no display is reachable (confirmed
// directly: it aborts the whole process via Qt's qFatal, which calls
// abort() -- a signal, not a throw, so no try/catch around main()'s own
// logic could ever have caught it). Running focus-monitor from a
// systemd unit with no desktop session attached, or over a plain SSH
// session without `-X`, hits exactly this -- the same headless
// deployment this project's own exit-code design (see README's
// Restart=on-failure discussion) is clearly meant to support.
inline bool DisplayAvailable(const char* display_env, const char* wayland_display_env) {
  const bool has_display = display_env != nullptr && display_env[0] != '\0';
  const bool has_wayland_display = wayland_display_env != nullptr && wayland_display_env[0] != '\0';
  return has_display || has_wayland_display;
}

#endif  // FOCUS_MONITOR_DISPLAY_AVAILABILITY_H_
