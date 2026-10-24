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
      const std::string command = "notify-send --urgency=normal -- \"" + EscapeForShell(title) +
                                   "\" \"" + EscapeForShell(message) + "\"";
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
