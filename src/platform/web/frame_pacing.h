// Android runtime scheduling; the shared game's tick rules remain unchanged.
#pragma once
#include <stdint.h>

inline unsigned WebFrameWaitMs(uint32_t now, uint32_t previous, bool focused,
                               bool frameSkip, bool skipDetected)
{
    if (!focused) return 100;
    if (frameSkip && skipDetected) return 1;
    const unsigned interval=frameSkip?33:16;
    const int32_t elapsed=int32_t(now-previous);
    return elapsed>=0 && unsigned(elapsed)<interval ? interval-unsigned(elapsed) : 1;
}
