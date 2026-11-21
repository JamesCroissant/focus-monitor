#ifndef FOCUS_MONITOR_NOTIFIER_H_
#define FOCUS_MONITOR_NOTIFIER_H_

#include <cstdlib>
#include <iostream>
#include <string>

// Escapes `text` for safe embedding inside a double-quoted shell argument,
// the way Notify() below embeds `title`/`message` into a notify-send
// command line. A free function rather than a private Notifier method
// specifically so this shell-safety-critical logic -- the one thing here
// that's actually dangerous to get wrong -- can be unit-tested on its
// own, the same way FocusTracker/AlertScheduler's logic already is,
// without constructing a Notifier or sending a real notification.
//
// `"`, `\`, `$`, and `` ` `` are the characters a POSIX shell still gives
// special meaning to *inside* double quotes (closing the string, command
// substitution, variable expansion); every other character, including
// shell metacharacters like `;`/`&`/`|`, is already literal once quoted
// and needs no escaping here.
inline std::string EscapeForShell(const std::string& text) {
  std::string escaped;
  escaped.reserve(text.size());
  for (char c : text) {
    if (c == '"' || c == '\\' || c == '$' || c == '`') {
      escaped.push_back('\\');
    }
    escaped.push_back(c);
  }
  return escaped;
}

// Builds the shell command Notify() below runs to deliver a desktop
// notification. Pulled out as its own function (like EscapeForShell
// above) specifically so its exact shape -- especially the trailing
// `&` that backgrounds it, and the output redirection that goes with
// backgrounding a command -- can be exercised directly with a stand-in
// command, without needing notify-send installed or actually sending a
// visible notification.
//
// The trailing `&` matters for more than tidiness: std::system() runs
// a full shell and blocks until that shell exits, and without `&` the
// shell doesn't exit until notify-send itself does. notify-send
// normally returns almost instantly, but it still talks to a D-Bus
// session and a separate notification-daemon process, either of which
// hanging (a stuck D-Bus call, an unresponsive daemon) would block
// Notify()'s caller for exactly as long as that hang lasts -- and
// Notify() is called from the same thread that drives the camera
// capture loop, so a hung notify-send would silently freeze frame
// capture, the preview window, and 'q'/ESC/Ctrl-C responsiveness along
// with it. `&` has the shell fork notify-send and return immediately
// without waiting for it, the same "best-effort, never block the real
// work on this" posture this class's own comment already states for
// notify-send being entirely optional. `>/dev/null 2>&1` goes with
// that: without it, notify-send's own stdout/stderr (normally silent,
// but not guaranteed to be) could still print to the terminal well
// after Notify() has returned and this process has moved on, now from
// a background job with no connection to anything currently printing.
inline std::string BuildNotifySendCommand(const std::string& command_name,
                                           const std::string& title, const std::string& message) {
  return command_name + " --urgency=normal -- \"" + EscapeForShell(title) + "\" \"" +
         EscapeForShell(message) + "\" >/dev/null 2>&1 &";
}

// Best-effort alert delivery: a terminal bell + log line always fire, and a
// Linux desktop notification (via `notify-send`) is sent additionally when
// that command is available on PATH. There is no hard dependency on any
// notification library, so this degrades gracefully on systems without
// notify-send (macOS, Windows, headless environments, ...).
class Notifier {
 public:
  Notifier() : notify_send_available_(CommandExists("notify-send")) {}

  void Notify(const std::string& title, const std::string& message) const {
    // '\a' is the ASCII bell; most terminals sound or flash on it.
    std::cout << '\a' << "[focus-monitor] " << title << ": " << message << std::endl;

    if (notify_send_available_) {
      const std::string command = BuildNotifySendCommand("notify-send", title, message);
      const int result = std::system(command.c_str());
      static_cast<void>(result);
    }
  }

 private:
  static bool CommandExists(const std::string& name) {
    const std::string check = "command -v " + name + " >/dev/null 2>&1";
    return std::system(check.c_str()) == 0;
  }

  bool notify_send_available_;
};

#endif  // FOCUS_MONITOR_NOTIFIER_H_
