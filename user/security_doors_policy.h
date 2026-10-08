#pragma once
#include <array>
#include <cstdint>

namespace SecurityDoors {
    enum class PinMode { None, Soft, Hard };
    constexpr unsigned KitchenMask = 1;
    constexpr unsigned ElectricalMask = 6;
    constexpr unsigned AllMask = KitchenMask | ElectricalMask;

    // Independent timers: opening the hallway must not restart an Electrical timer.
    class PinTimer {
        std::array<int64_t, 3> openedAt{ -1, -1, -1 };
    public:
        void Reset() { openedAt.fill(-1); }
        unsigned Update(PinMode mode, const std::array<bool, 3>& open, int64_t now) {
            unsigned close = 0;
            for (unsigned i = 0; i < open.size(); ++i) {
                if (mode == PinMode::None || !open[i]) {
                    openedAt[i] = -1;
                }
                else if (mode == PinMode::Hard) {
                    close |= 1u << i;
                    openedAt[i] = -1;
                }
                else {
                    if (openedAt[i] < 0) openedAt[i] = now;
                    if (now - openedAt[i] >= 1500) close |= 1u << i;
                }
            }
            return close;
        }
    };
}
