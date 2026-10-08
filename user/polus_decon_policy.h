#pragma once
#include <cstdint>
#include <array>

namespace PolusDecon {
    constexpr bool AnyOpen(const std::array<bool, 2>& doors) { return doors[0] || doors[1]; }
    template<typename Door, typename Setter>
    void SetPair(const std::array<Door*, 2>& doors, bool open, Setter set) {
        for (auto door : doors) set(door, open);
    }

    class PinTimer {
        int64_t openedAt = -1;
    public:
        void Reset() { openedAt = -1; }
        bool Update(bool hard, bool soft, uint32_t cycle, const std::array<bool, 2>& doors, int64_t now) {
            if (hard) { Reset(); return cycle != 0 || AnyOpen(doors); }
            // A manual pair-open intentionally leaves the native cycle Idle.
            if (!soft || !AnyOpen(doors)) { Reset(); return false; }
            if (openedAt < 0) openedAt = now;
            return now - openedAt >= 1500;
        }
    };
}
