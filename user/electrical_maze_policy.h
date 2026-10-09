#pragma once
#include <cstdint>

namespace ElectricalMaze {
    struct Command { unsigned door; bool open; uint64_t generation; };
    constexpr bool CanApply(const Command& command, uint64_t generation,
        unsigned count, bool host) {
        return command.generation == generation && command.door < count && host;
    }
    // Only the selected reference is changed. Serialization reads all other
    // door states from the live system, preserving the rest of the maze.
    template<class Doors, class Setter>
    bool ApplySelected(const Command& command, uint64_t generation,
        bool host, const Doors& doors, Setter set) {
        if (!CanApply(command, generation, static_cast<unsigned>(doors.size()), host)) return false;
        return set(doors[command.door], command.open);
    }
}
