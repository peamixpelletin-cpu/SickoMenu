#pragma once
#include <cstdint>

namespace KitchenEast {
    enum class PinMode { None, Soft, Hard };
    class PinTimer {
        int64_t openedAt = -1;
    public:
        void Reset() { openedAt = -1; }
        bool Update(PinMode mode, bool open, int64_t now) {
            if (mode == PinMode::None || !open) { Reset(); return false; }
            if (mode == PinMode::Hard) { Reset(); return true; }
            if (openedAt < 0) openedAt = now;
            return now - openedAt >= 1500;
        }
    };
}
