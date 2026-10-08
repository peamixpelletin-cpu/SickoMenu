#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace ElectricalMaze {
    constexpr unsigned DoorCount = 12;
    struct DoorState { float x = 0, y = 0; bool open = false; };
    struct Snapshot {
        std::array<DoorState, DoorCount> doors{};
        uint64_t generation = 0;
        bool ready = false, host = false, experimental = false;
        std::string status;
    };
    Snapshot Read();
    void EnableExperimental(bool enabled);
    void Queue(unsigned door, bool open, uint64_t generation);
    void Update(); // Unity/game thread only; UI reads snapshots and queues commands.
    void Reset();
}
