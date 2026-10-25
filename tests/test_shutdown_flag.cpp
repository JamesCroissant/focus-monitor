#include <csignal>

#include "../src/shutdown_flag.h"
#include "test_framework.h"

namespace {

void FlagStartsUnsetAfterReset() {
  ShutdownFlag::Instance().ResetForTesting();
  CHECK(!ShutdownFlag::Instance().Requested());
}

void RequestSetsTheFlag() {
  ShutdownFlag::Instance().ResetForTesting();
  ShutdownFlag::Instance().Request();
  CHECK(ShutdownFlag::Instance().Requested());
}

// The real thing main() relies on: that installing the handler and then
// actually receiving SIGINT (not just calling Request() directly) sets
// the flag. raise() delivers the signal synchronously on this thread, so
// by the time it returns, our handler has already run.
void SigintActuallySetsTheFlagThroughTheRealHandler() {
  ShutdownFlag::Instance().ResetForTesting();
  InstallShutdownSignalHandlers();

  raise(SIGINT);

  CHECK(ShutdownFlag::Instance().Requested());
}

void SigtermActuallySetsTheFlagThroughTheRealHandler() {
  ShutdownFlag::Instance().ResetForTesting();
  InstallShutdownSignalHandlers();

  raise(SIGTERM);

  CHECK(ShutdownFlag::Instance().Requested());
}

}  // namespace

int main() {
  RUN_TEST(FlagStartsUnsetAfterReset);
  RUN_TEST(RequestSetsTheFlag);
  RUN_TEST(SigintActuallySetsTheFlagThroughTheRealHandler);
  RUN_TEST(SigtermActuallySetsTheFlagThroughTheRealHandler);
  return test_framework::FailureCount() == 0 ? 0 : 1;
}
