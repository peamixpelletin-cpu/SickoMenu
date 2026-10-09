#include "pch-il2cpp.h"
#include "electrical_maze.h"
#include "electrical_maze_policy.h"
#include "game.h"
#include "state.hpp"
#include "utility.h"
#include <cstring>
#include <deque>
#include <mutex>

namespace ElectricalMaze {
    namespace {
        std::mutex mutex;
        Snapshot snapshot;
        std::deque<Command> commands;
        ShipStatus* currentShip = nullptr;

        struct Maze {
            Il2CppObject* system = nullptr;
            FieldInfo* dirty = nullptr;
            std::array<Il2CppObject*, DoorCount> doors{};
            std::array<FieldInfo*, DoorCount> openFields{};
            std::array<const MethodInfo*, DoorCount> setters{};

            bool Resolve() {
                auto ship = *Game::pShipStatus;
                if (!ship->fields.Systems) return false;
                il2cpp::Dictionary systems(ship->fields.Systems);
                system = reinterpret_cast<Il2CppObject*>(systems[SystemTypes__Enum::Decontamination]);
                if (!system || !system->klass || std::strcmp(system->klass->name, "ElectricalDoors") != 0) return false;
                auto field = il2cpp_class_get_field_from_name(system->klass, "Doors");
                Il2CppArraySize* array = nullptr;
                if (field) il2cpp_field_get_value(system, field, &array);
                if (!array || array->max_length != DoorCount) return false;
                dirty = il2cpp_class_get_field_from_name(system->klass, "<IsDirty>k__BackingField");
                if (!dirty) return false;
                for (unsigned i = 0; i < DoorCount; ++i) {
                    doors[i] = reinterpret_cast<Il2CppObject*>(array->vector[i]);
                    if (!doors[i] || !doors[i]->klass || std::strcmp(doors[i]->klass->name, "StaticDoor") != 0) return false;
                    for (unsigned j = 0; j < i; ++j) if (doors[j] == doors[i]) return false;
                    openFields[i] = il2cpp_class_get_field_from_name(doors[i]->klass, "<IsOpen>k__BackingField");
                    setters[i] = il2cpp_class_get_method_from_name(doors[i]->klass, "SetOpen", 1);
                    if (!openFields[i] || !setters[i]) return false;
                }
                return true;
            }
            bool Capture() const {
                for (unsigned i = 0; i < DoorCount; ++i) {
                    auto transform = app::Component_get_transform(reinterpret_cast<Component_1*>(doors[i]), nullptr);
                    if (!transform) return false;
                    auto position = app::Transform_get_position(transform, nullptr);
                    snapshot.doors[i].x = position.x;
                    snapshot.doors[i].y = position.y;
                    il2cpp_field_get_value(doors[i], openFields[i], &snapshot.doors[i].open);
                }
                return true;
            }
            bool Set(unsigned i, bool open) const {
                void* args[] = { &open };
                Il2CppException* exception = nullptr;
                il2cpp_runtime_invoke(setters[i], doors[i], args, &exception);
                return !exception;
            }

        };
        void Clear() {
            const auto generation = snapshot.generation + 1;
            snapshot = {};
            snapshot.generation = generation;
            commands.clear();
            currentShip = nullptr;
        }
    }
    Snapshot Read() { std::lock_guard lock(mutex); return snapshot; }
    void Queue(unsigned door, bool open, uint64_t generation) {
        std::lock_guard lock(mutex);
        Command command{ door, open, generation };
        if (snapshot.ready && CanApply(command, snapshot.generation, DoorCount, snapshot.host)
            && commands.size() < 32) commands.push_back(command);
    }
    void Reset() { std::lock_guard lock(mutex); Clear(); }
    void Update() {
        std::lock_guard lock(mutex);
        if (State.PanicMode || !IsInGame() || State.mapType != Settings::MapType::Airship ||
            !Game::pShipStatus || !*Game::pShipStatus || State.InMeeting || State.InExileUI) {
            if (currentShip || snapshot.ready || !commands.empty()) Clear();
            return;
        }
        auto ship = *Game::pShipStatus;
        if (currentShip != ship) { Clear(); currentShip = ship; }
        Maze maze;
        snapshot.ready = maze.Resolve();
        snapshot.host = IsHost();
        if (!snapshot.ready) { commands.clear(); return; }
        snapshot.ready = maze.Capture();
        if (!snapshot.ready) { commands.clear(); return; }
        // Only host commands can mutate the maze or mark it for replication.
        if (!commands.empty()) {
            auto command = commands.front(); commands.pop_front();
            const bool applied = ApplySelected(command, snapshot.generation, snapshot.host,
                maze.doors, [&](Il2CppObject*, bool open) { return maze.Set(command.door, open); });
            if (applied) {
                bool dirty = true;
                il2cpp_field_set_value(maze.system, maze.dirty, &dirty);
                snapshot.status = "Host update queued.";
                snapshot.ready = maze.Capture();
            }
        }
    }
}
