#include "pch-il2cpp.h"
#include "polus_decon.h"
#include "polus_decon_policy.h"
#include "game.h"
#include "state.hpp"
#include "utility.h"
#include "toasts.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>

namespace PolusDecon {
    namespace {
        constexpr std::array rooms{ SystemTypes__Enum::Decontamination,
            SystemTypes__Enum::Decontamination2, SystemTypes__Enum::Decontamination3 };
        std::array<PinTimer, 3> timers;
        std::array<int64_t, 3> lastClientClose{ -1, -1, -1 };

        bool Contains(const std::vector<SystemTypes__Enum>& list, SystemTypes__Enum room) {
            return std::find(list.begin(), list.end(), room) != list.end();
        }
        Il2CppObject* System(SystemTypes__Enum room) {
            if (!IsGroup(room) || !Game::pShipStatus || !*Game::pShipStatus ||
                !(*Game::pShipStatus)->fields.Systems) return nullptr;
            il2cpp::Dictionary systems((*Game::pShipStatus)->fields.Systems);
            auto object = reinterpret_cast<Il2CppObject*>(systems[room]);
            return object && object->klass && object->klass->name &&
                std::strcmp(object->klass->name, "DeconSystem") == 0 ? object : nullptr;
        }
        Il2CppObject* Reference(Il2CppObject* object, const char* name) {
            auto field = il2cpp_class_get_field_from_name(object->klass, name);
            Il2CppObject* value = nullptr;
            if (field) il2cpp_field_get_value(object, field, &value);
            return value;
        }
        bool Position(Il2CppObject* object, Vector3& position) {
            if (!object) return false;
            auto transform = app::Component_get_transform(reinterpret_cast<Component_1*>(object), nullptr);
            if (!transform) return false;
            position = app::Transform_get_position(transform, nullptr);
            return true;
        }
        struct Chamber {
            SystemTypes__Enum room{};
            Il2CppObject* system = nullptr;
            Il2CppObject* doorSystem = nullptr;
            FieldInfo* state = nullptr;
            FieldInfo* time = nullptr;
            FieldInfo* dirty = nullptr;
            FieldInfo* doorsDirty = nullptr;
            const MethodInfo* updateDoors = nullptr;
            const MethodInfo* serializeCycle = nullptr;
            const MethodInfo* serializeDoors = nullptr;
            std::array<PlainDoor*, 2> doors{};
            Vector3 upper{}, lower{};
            uint32_t StateValue() const {
                uint32_t value = 0;
                il2cpp_field_get_value(system, state, &value);
                return value;
            }
            std::array<bool, 2> DoorStates() const {
                return { doors[0]->fields.Open, doors[1]->fields.Open };
            }
            bool RequestClient(bool open) const {
                if (!Game::pLocalPlayer || !*Game::pLocalPlayer) return false;
                auto ship = *Game::pShipStatus;
                const auto physicalRoom = doors[0]->fields._.Room;
                if (doors[1]->fields._.Room != physicalRoom) return false;
                std::vector<ClientDoor> physical;
                std::array<PlainDoor*, 32> byId{};
                unsigned index = 0;
                for (auto door : il2cpp::Array(ship->fields.AllDoors)) {
                    const auto currentIndex = index++;
                    if (!door || door->fields.Room != physicalRoom) continue;
                    if (!door->klass || std::strcmp(door->klass->name, "PlainDoor") != 0 ||
                        door->fields.Id < 0 || door->fields.Id > 31 ||
                        static_cast<unsigned>(door->fields.Id) != currentIndex) return false;
                    auto plain = reinterpret_cast<PlainDoor*>(door);
                    byId[door->fields.Id] = plain;
                    bool restoreOpen = plain->fields.Open;
                    SystemTypes__Enum otherRoom{};
                    if (RoomForDoor(door, otherRoom) && Contains(State.pinnedDoors, otherRoom)) restoreOpen = false;
                    physical.push_back({ door->fields.Id, restoreOpen, plain == doors[0] || plain == doors[1] });
                }
                const auto plan = PlanClientRequest(open, physical);
                if (!plan.valid) return false;
                // This is the original official utility's client RPC path, not
                // ShipStatus Data spoofing. The real host retains authority over
                // decon cycle timing, so an active cycle can override the result.
                if (plan.closeRoom) app::ShipStatus_RpcCloseDoorsOfType(ship, physicalRoom, nullptr);
                for (unsigned i = 0; i < plan.count; ++i) {
                    app::ShipStatus_RpcUpdateSystem(ship, SystemTypes__Enum::Doors,
                        static_cast<uint8_t>(plan.openIds[i] | 64), nullptr);
                    app::PlainDoor_SetDoorway(byId[plan.openIds[i]], true, nullptr);
                }
                SetPair(doors, open, [](PlainDoor* door, bool value) {
                    app::PlainDoor_SetDoorway(door, value, nullptr);
                });
                return true;
            }
            bool Apply(bool open) const {
                if (!IsHost()) return RequestClient(open);
                uint8_t value = 0;
                float duration = 0.f;
                il2cpp_field_set_value(system, state, &value);
                il2cpp_field_set_value(system, time, &duration);
                Il2CppException* exception = nullptr;
                il2cpp_runtime_invoke(updateDoors, system, nullptr, &exception);
                if (exception) return false;
                SetPair(doors, open, [](PlainDoor* door, bool value) {
                    app::PlainDoor_SetDoorway(door, value, nullptr);
                });
                bool changed = true;
                il2cpp_field_set_value(doorSystem, doorsDirty, &changed);

                // Cancel the chamber cycle BEFORE applying both physical door
                // states on peers. Normal ShipStatus serialization visits Doors
                // before DeconSystem and would otherwise close the pair again.
                if (IsInMultiplayerGame()) {
                    auto writer = app::MessageWriter_Get(SendOption__Enum::Reliable, nullptr);
                    if (!writer) return false;
                    app::MessageWriter_StartMessage(writer, 5, nullptr); // GameData
                    app::MessageWriter_WriteInt32(writer, (*Game::pAmongUsClient)->fields._.GameId, nullptr);
                    app::MessageWriter_StartMessage(writer, 1, nullptr); // ShipStatus data
                    app::MessageWriter_WritePacked(writer, (*Game::pShipStatus)->fields._.NetId, nullptr);
                    bool initial = false;
                    void* args[] = { writer, &initial };
                    app::MessageWriter_StartMessage(writer, static_cast<uint8_t>(room), nullptr);
                    il2cpp_runtime_invoke(serializeCycle, system, args, &exception);
                    app::MessageWriter_EndMessage(writer, nullptr);
                    if (!exception) {
                        app::MessageWriter_StartMessage(writer, static_cast<uint8_t>(SystemTypes__Enum::Doors), nullptr);
                        il2cpp_runtime_invoke(serializeDoors, doorSystem, args, &exception);
                        app::MessageWriter_EndMessage(writer, nullptr);
                    }
                    app::MessageWriter_EndMessage(writer, nullptr);
                    app::MessageWriter_EndMessage(writer, nullptr);
                    if (!exception) app::InnerNetClient_SendOrDisconnect(
                        reinterpret_cast<InnerNetClient*>(*Game::pAmongUsClient), writer, nullptr);
                    app::MessageWriter_Recycle(writer, nullptr);
                    if (exception) return false;
                }
                // Do not let a later ordinary update re-send Idle after the pair.
                changed = false;
                il2cpp_field_set_value(system, dirty, &changed);
                return true;
            }
        };
        bool Resolve(SystemTypes__Enum room, Chamber& chamber) {
            chamber.room = room;
            chamber.system = System(room);
            if (!chamber.system) return false;
            auto klass = chamber.system->klass;
            chamber.state = il2cpp_class_get_field_from_name(klass, "<CurState>k__BackingField");
            chamber.time = il2cpp_class_get_field_from_name(klass, "timer");
            chamber.dirty = il2cpp_class_get_field_from_name(klass, "<IsDirty>k__BackingField");
            chamber.updateDoors = il2cpp_class_get_method_from_name(klass, "UpdateDoorsViaState", 0);
            chamber.serializeCycle = il2cpp_class_get_method_from_name(klass, "Serialize", 2);
            auto ship = *Game::pShipStatus;
            if (!ship->fields.AllDoors) return false;
            il2cpp::Dictionary systems(ship->fields.Systems);
            chamber.doorSystem = reinterpret_cast<Il2CppObject*>(systems[SystemTypes__Enum::Doors]);
            if (!chamber.doorSystem || !chamber.doorSystem->klass ||
                std::strcmp(chamber.doorSystem->klass->name, "DoorsSystemType") != 0) return false;
            chamber.doorsDirty = il2cpp_class_get_field_from_name(chamber.doorSystem->klass, "<IsDirty>k__BackingField");
            chamber.serializeDoors = il2cpp_class_get_method_from_name(chamber.doorSystem->klass, "Serialize", 2);
            const std::array references{ Reference(chamber.system, "UpperDoor"), Reference(chamber.system, "LowerDoor") };
            for (size_t i = 0; i < references.size(); ++i) {
                for (auto door : il2cpp::Array(ship->fields.AllDoors)) {
                    if (reinterpret_cast<Il2CppObject*>(door) == references[i] && door && door->klass &&
                        std::strcmp(door->klass->name, "PlainDoor") == 0)
                        chamber.doors[i] = reinterpret_cast<PlainDoor*>(door);
                }
            }
            return chamber.state && chamber.time && chamber.dirty && chamber.doorsDirty && chamber.updateDoors &&
                chamber.serializeCycle && chamber.serializeDoors && chamber.doors[0] && chamber.doors[1] &&
                chamber.doors[0] != chamber.doors[1] &&
                Position(references[0], chamber.upper) && Position(references[1], chamber.lower);
        }
        void Unavailable() {
            Toasts::AddToast("Decontamination", "Controls are unavailable for this Polus layout.");
        }
    }

    bool IsGroup(SystemTypes__Enum room) {
        return State.mapType == Settings::MapType::Pb &&
            std::find(rooms.begin(), rooms.end(), room) != rooms.end();
    }
    void RefreshEntries() {
        // All four Polus PlainDoors share Room=Decontamination. The two systems'
        // references identify the separate pairs; Room alone cannot separate them.
        for (auto room : rooms)
            if (System(room) && !Contains(State.mapDoors, room)) State.mapDoors.push_back(room);
        std::sort(State.mapDoors.begin(), State.mapDoors.end());
    }
    const char* Label(SystemTypes__Enum room) {
        Chamber current;
        if (Resolve(room, current)) {
            const float y = current.upper.y + current.lower.y;
            for (auto other : rooms) {
                if (other == room) continue;
                Chamber comparison;
                if (Resolve(other, comparison)) return y > comparison.upper.y + comparison.lower.y ?
                    "Decontamination (Upper)" : "Decontamination (Lower)";
            }
        }
        // Keep distinct selectable entries even if a custom layout lacks metadata.
        return room == rooms[0] ? "Decontamination (System 1)" :
            room == rooms[1] ? "Decontamination (System 2)" : "Decontamination (System 3)";
    }
    bool ReadState(SystemTypes__Enum room, bool& anyOpen) {
        Chamber chamber;
        if (!Resolve(room, chamber)) return false;
        anyOpen = AnyOpen(chamber.DoorStates());
        return true;
    }
    bool CanControl(SystemTypes__Enum room, bool notify) {
        if (State.PanicMode) return false;
        if (IsInMultiplayerGame() && (!Game::pAmongUsClient || !*Game::pAmongUsClient)) return false;
        Chamber chamber;
        if (Resolve(room, chamber)) return true;
        if (notify) Unavailable();
        return false;
    }
    bool IsHardPinned(SystemTypes__Enum room) {
        return !State.PanicMode && IsHost() && IsGroup(room) && Contains(State.pinnedDoors, room);
    }
    bool RoomForDoor(OpenableDoor* door, SystemTypes__Enum& room) {
        if (!door || State.mapType != Settings::MapType::Pb) return false;
        for (auto candidate : rooms) {
            auto system = System(candidate);
            if (system && (Reference(system, "UpperDoor") == reinterpret_cast<Il2CppObject*>(door) ||
                           Reference(system, "LowerDoor") == reinterpret_cast<Il2CppObject*>(door))) {
                room = candidate;
                return true;
            }
        }
        return false;
    }
    bool IsPhysicalDoor(OpenableDoor* door) {
        SystemTypes__Enum room{};
        return RoomForDoor(door, room);
    }
    bool IsHardPinnedDoor(OpenableDoor* door) {
        SystemTypes__Enum room{};
        return RoomForDoor(door, room) && IsHardPinned(room);
    }
    bool SetOpen(SystemTypes__Enum room, bool open) {
        if (!CanControl(room, true) || (open && Contains(State.pinnedDoors, room))) return false;
        Chamber chamber;
        if (!Resolve(room, chamber)) return false;
        Reset(room);
        if (!chamber.Apply(open)) { Unavailable(); return false; }
        return true;
    }
    void UpdatePins() {
        if (State.PanicMode || State.mapType != Settings::MapType::Pb ||
            !IsInGame() || State.InMeeting || State.InExileUI) { Reset(); return; }
        const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        for (size_t i = 0; i < rooms.size(); ++i) {
            const bool hard = Contains(State.pinnedDoors, rooms[i]);
            const bool soft = Contains(State.softPinnedDoors, rooms[i]);
            if (!hard && !soft) { timers[i].Reset(); lastClientClose[i] = -1; continue; }
            Chamber chamber;
            if (!Resolve(rooms[i], chamber)) { timers[i].Reset(); continue; }
            // A client cannot cancel the host's cycle. Act on observed open
            // doors only, instead of continuously retrying against cycle state.
            if (!IsHost() && lastClientClose[i] >= 0 && now - lastClientClose[i] < 500) continue;
            if (timers[i].Update(hard, soft, IsHost() ? chamber.StateValue() : 0, chamber.DoorStates(), now)) {
                chamber.Apply(false);
                if (!IsHost()) lastClientClose[i] = now;
                timers[i].Reset();
            }
        }
    }
    void Reset(SystemTypes__Enum room) {
        const auto found = std::find(rooms.begin(), rooms.end(), room);
        if (found != rooms.end()) {
            const auto index = static_cast<size_t>(found - rooms.begin());
            timers[index].Reset();
            lastClientClose[index] = -1;
        }
    }
    void Reset() { for (auto& timer : timers) timer.Reset(); lastClientClose.fill(-1); }
}
