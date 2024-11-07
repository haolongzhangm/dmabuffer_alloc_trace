#include <pthread.h>

#include "debug_disable.h"

pthread_key_t g_disable_key;

bool DebugCallsDisabled() {
  if (pthread_getspecific(g_disable_key) != nullptr) {
    return true;
  }
  return false;
}

bool DebugDisableInitialize() { 
  int error = pthread_key_create(&g_disable_key, nullptr);
  if (error != 0) {
    return false;
  }
  pthread_setspecific(g_disable_key, nullptr);

  return true;
}

void DebugDisableFinalize() {
  pthread_key_delete(g_disable_key);
}

void DebugDisableSet(bool disable) {
  if (disable) {
    pthread_setspecific(g_disable_key, reinterpret_cast<void*>(1));
  } else {
    pthread_setspecific(g_disable_key, nullptr);
  }
}
