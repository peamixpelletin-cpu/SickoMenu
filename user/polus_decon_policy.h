#pragma once
#include <cstdint>
#include <array>

namespace PolusDecon {
    constexpr bool ClientHardPinDue(int64_t now, int64_t lastClose, bool queueEmpty) {
        return queueEmpty && (lastClose < 0 || now - lastClose >= 50);
    }
    struct ClientDoor { int id; bool open; bool selected; };
    struct ClientPlan {
        bool valid = false, closeRoom = false;
        std::array<uint8_t, 32> openIds{};
        unsigned count = 0;
    };
    template<class Doors>
    ClientPlan PlanClientRequest(bool open, const Doors& doors) {
        ClientPlan plan;
        uint32_t seen = 0;
        unsigned selected = 0;
        for (const auto& door : doors) {
            if (door.id < 0 || door.id > 31 || (seen & (1u << door.id))) return {};
            seen |= 1u << door.id;
            if (door.selected) ++selected;
            // Open has individual IDs; close only has a shared room command.
            // Reopen previously open non-selected doors after the room close.
            if ((open && door.selected) || (!open && !door.selected && door.open))
                plan.openIds[plan.count++] = static_cast<uint8_t>(door.id);
        }
        if (selected != 2) return {};
        plan.valid = true;
        plan.closeRoom = !open;
        return plan;
    }
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
