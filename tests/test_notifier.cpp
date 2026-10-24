#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "../src/notifier.h"
#include "test_framework.h"

namespace {

void PlainTextIsUnchanged() {
  CHECK(EscapeForShell("hello world") == "hello world");
  CHECK(EscapeForShell("") == "");
}

void EscapesDoubleQuote() {
  CHECK(EscapeForShell("say \"hi\"") == "say \\\"hi\\\"");
}

void EscapesBackslash() {
  CHECK(EscapeForShell("a\\b") == "a\\\\b");
}

void EscapesDollarSign() {
  CHECK(EscapeForShell("$HOME") == "\\$HOME");
}

void EscapesBacktick() {
  CHECK(EscapeForShell("`whoami`") == "\\`whoami\\`");
}

// Other shell metacharacters are already literal once inside double
// quotes, so EscapeForShell must leave them alone -- escaping them too
// would just change what gets displayed in the notification, for no
// safety benefit.
void LeavesOtherShellMetacharactersAlone() {
  CHECK(EscapeForShell("a;b&c|d*e?f") == "a;b&c|d*e?f");
}

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream in(path);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

// The real test: actually build the same "<cmd> \"<escaped>\"" shell
// command Notify() builds (with `printf` standing in for `notify-send`,
// so this test doesn't depend on notify-send being installed), run it
// for real, and confirm the string that comes back out the other side
// of the shell's own double-quote parsing is byte-for-byte the original
// -- not just that EscapeForShell's output "looks" escaped.
void EscapedTextRoundTripsThroughARealShellDoubleQuotedArgument() {
  const std::filesystem::path out_path =
      std::filesystem::temp_directory_path() / "focus_monitor_test_notifier_roundtrip.txt";
  std::filesystem::remove(out_path);

  const std::string raw = "a \"quoted\" $VAR `cmd` \\backslash and a % percent sign";
  const std::string command =
      "printf '%s' \"" + EscapeForShell(raw) + "\" > " + out_path.string();
  const int result = std::system(command.c_str());
  CHECK(result == 0);
  CHECK(ReadFile(out_path) == raw);

  std::filesystem::remove(out_path);
}

// A direct injection attempt: if EscapeForShell ever failed to escape a
// double quote, this payload's embedded `"` would close the shell's
// quoting early and let `; touch <marker>; echo "` run as a second,
// unintended command. Confirms no such side effect occurs, not just that
// the round trip produces the "right-looking" string.
void MaliciousInputCannotEscapeTheQuotedArgument() {
  const std::filesystem::path marker =
      std::filesystem::temp_directory_path() / "focus_monitor_test_notifier_injection_marker";
  std::filesystem::remove(marker);

  const std::string payload = "foo\"; touch " + marker.string() + "; echo \"bar";
  const std::string command = "printf '%s' \"" + EscapeForShell(payload) + "\" > /dev/null";
  const int result = std::system(command.c_str());
  static_cast<void>(result);

  CHECK(!std::filesystem::exists(marker));
}

}  // namespace

int main() {
  RUN_TEST(PlainTextIsUnchanged);
  RUN_TEST(EscapesDoubleQuote);
  RUN_TEST(EscapesBackslash);
  RUN_TEST(EscapesDollarSign);
  RUN_TEST(EscapesBacktick);
  RUN_TEST(LeavesOtherShellMetacharactersAlone);
  RUN_TEST(EscapedTextRoundTripsThroughARealShellDoubleQuotedArgument);
  RUN_TEST(MaliciousInputCannotEscapeTheQuotedArgument);
  return test_framework::FailureCount() == 0 ? 0 : 1;
}
