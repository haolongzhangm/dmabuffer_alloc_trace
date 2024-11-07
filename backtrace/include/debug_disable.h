#pragma once
#include <sys/cdefs.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <reserved_signals.h>

// =============================================================================
// Used to disable the debug allocation calls.
// =============================================================================
bool DebugDisableInitialize();
void DebugDisableFinalize();

bool DebugCallsDisabled();
void DebugDisableSet(bool disable);

class ScopedDisableDebugCalls {
 public:
  ScopedDisableDebugCalls() : disabled_(DebugCallsDisabled()) {
    if (!disabled_) {
      DebugDisableSet(true);
    }
  }
  ~ScopedDisableDebugCalls() {
    if (!disabled_) {
      DebugDisableSet(false);
    }
  }

 private:
  bool disabled_;

  BIONIC_DISALLOW_COPY_AND_ASSIGN(ScopedDisableDebugCalls);
};