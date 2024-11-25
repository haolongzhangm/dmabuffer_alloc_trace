#include "DebugData.h"

bool DebugData::Initialize(void* storage) {
    if (!config_.Init()) {
        return false;
    }

    pointer.reset(new (storage) PointerData());
    if (!pointer->Initialize(config_)) {
      return false;
    }

    return true;
}