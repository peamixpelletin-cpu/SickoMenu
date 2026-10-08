#pragma once
#include <cstdint>

namespace PolusDecon {
    constexpr bool IsOpenState(uint32_t state) { return (state & (1u | 4u)) != 0; }
    // Native Enter states open the requested entry side, then run the normal cycle.
    constexpr uint32_t EntryState(bool upperSide) { return upperSide ? 1u : 9u; }

    class PinTimer {
        int64_t openedAt = -1;
        uint32_t previousState = 0;
    public:
        void Reset() { openedAt = -1; previousState = 0; }
        bool Update(bool hard, bool soft, uint32_t state, int64_t now) {
            if (hard) { Reset(); return state != 0; }
            if (!soft || !IsOpenState(state)) { Reset(); return false; }
            if (openedAt < 0 || previousState != state) openedAt = now;
            previousState = state;
            return now - openedAt >= 1500;
        }
    };
}
