#pragma once

#include <macros.h>

#include "Config.h"
#include "PointerData.h"

class DebugData {
public:
    DebugData() = default;
    ~DebugData() = default;

    bool Initialize(void* storage);

    const Config& config() { return config_; }

    bool TrackPointers() { return config_.options() & TRACK_ALLOCS; }

    std::unique_ptr<PointerData> pointer;

private:
    Config config_;

    BIONIC_DISALLOW_COPY_AND_ASSIGN(DebugData);
};

extern DebugData* g_debug;
