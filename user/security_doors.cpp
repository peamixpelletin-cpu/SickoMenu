#include "pch-il2cpp.h"
#include "security_doors.h"
#include "security_doors_policy.h"
#include "game.h"
#include "state.hpp"
#include "utility.h"
#include "toasts.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>

namespace SecurityDoors {
    namespace {
        PinTimer timer;

        bool Contains(const std::vector<SystemTypes__Enum>& rooms, SystemTypes__Enum room) {
            return std::find(rooms.begin(), rooms.end(), room) != rooms.end();
        }

        bool ClassMatches(Il2CppClass* klass, const char* name) {
            for (auto k = klass; k; k = k->parent)
                if (k->name && std::strcmp(k->name, name) == 0) return true;
            return false;
        }

        FieldInfo* Field(Il2CppObject* object, const char* name) {
            return object && object->klass ? il2cpp_class_get_field_from_name(object->klass, name) : nullptr;
        }

        Il2CppObject* Reference(Il2CppObject* object, const char* name) {
            auto field = Field(object, name);
            Il2CppObject* value = nullptr;
            if (field) il2cpp_field_get_value(object, field, &value);
            return value;
        }

        struct Group {
            PlainDoor* kitchen = nullptr;
            Il2CppObject* normalSystem = nullptr;
            Il2CppObject* electricalSystem = nullptr;
            FieldInfo* normalDirty = nullptr;
            FieldInfo* electricalDirty = nullptr;
            std::array<Il2CppObject*, 2> exits{};
            std::array<FieldInfo*, 2> exitOpen{};
            std::array<const MethodInfo*, 2> exitSetOpen{};

            std::array<bool, 3> State() const {
                std::array<bool, 3> result{ kitchen->fields.Open, false, false };
                for (unsigned i = 0; i < exits.size(); ++i)
                    il2cpp_field_get_value(exits[i], exitOpen[i], &result[i + 1]);
                return result;
            }

            bool Apply(unsigned mask, bool open) const {
                bool success = true;
                bool dirty = true;
                if (mask & KitchenMask) {
                    app::PlainDoor_SetDoorway(kitchen, open, nullptr);
                    il2cpp_field_set_value(normalSystem, normalDirty, &dirty);
                }
                for (unsigned i = 0; i < exits.size(); ++i) {
                    if (!(mask & (1u << (i + 1)))) continue;
                    void* args[] = { &open };
                    Il2CppException* exception = nullptr;
                    il2cpp_runtime_invoke(exitSetOpen[i], exits[i], args, &exception);
                    if (exception) success = false;
                    // The host's ordinary ShipStatus serialization sends the door
                    // states. Do not send StaticDoor indices as normal Doors RPC IDs.
                    il2cpp_field_set_value(electricalSystem, electricalDirty, &dirty);
                }
                return success;
            }
        };

        bool Resolve(Group& group) {
            if (State.mapType != Settings::MapType::Airship || !Game::pShipStatus || !*Game::pShipStatus)
                return false;
            auto ship = *Game::pShipStatus;
            if (!ship->fields.AllDoors || !ship->fields.Systems) return false;
            for (auto door : il2cpp::Array(ship->fields.AllDoors)) {
                if (IsKitchenDoor(door) && ClassMatches(door->klass, "PlainDoor")) {
                    group.kitchen = reinterpret_cast<PlainDoor*>(door);
                    break;
                }
            }
            il2cpp::Dictionary systems(ship->fields.Systems);
            group.normalSystem = reinterpret_cast<Il2CppObject*>(systems[SystemTypes__Enum::Doors]);
            group.electricalSystem = reinterpret_cast<Il2CppObject*>(systems[SystemTypes__Enum::Decontamination]);
            if (!group.kitchen || !group.normalSystem || !group.electricalSystem ||
                !ClassMatches(group.normalSystem->klass, "DoorsSystemType") ||
                !ClassMatches(group.electricalSystem->klass, "ElectricalDoors")) return false;
            group.normalDirty = Field(group.normalSystem, "<IsDirty>k__BackingField");
            group.electricalDirty = Field(group.electricalSystem, "<IsDirty>k__BackingField");
            if (!group.normalDirty || !group.electricalDirty) return false;

            // Resolve the game's actual LeftExits references rather than relying on
            // the published 10/11 indices or assuming StaticDoor inherits PlainDoor.
            auto leftExits = Reference(group.electricalSystem, "LeftExits");
            auto exits = reinterpret_cast<Il2CppArraySize*>(Reference(leftExits, "Doors"));
            if (!exits || exits->max_length != 2) return false;
            for (unsigned i = 0; i < group.exits.size(); ++i) {
                auto exit = reinterpret_cast<Il2CppObject*>(exits->vector[i]);
                if (!exit || !ClassMatches(exit->klass, "StaticDoor")) return false;
                group.exits[i] = exit;
                group.exitOpen[i] = Field(exit, "<IsOpen>k__BackingField");
                group.exitSetOpen[i] = il2cpp_class_get_method_from_name(exit->klass, "SetOpen", 1);
                if (!group.exitOpen[i] || !group.exitSetOpen[i]) return false;
            }
            return group.exits[0] != group.exits[1];
        }

        void Unavailable() {
            Toasts::AddToast("Security doors", "Security controls are unavailable for this Airship layout.");
        }
    }

    bool IsGroup(SystemTypes__Enum room) {
        return State.mapType == Settings::MapType::Airship && room == SystemTypes__Enum::Security;
    }

    bool IsKitchenDoor(OpenableDoor* door) {
        return IsGroup(SystemTypes__Enum::Security) && door &&
            door->fields.Room == SystemTypes__Enum::Kitchen && door->fields.Id == 9;
    }

    bool IsHardPinnedKitchen(OpenableDoor* door) {
        return !State.PanicMode && IsHost() && IsKitchenDoor(door) &&
            Contains(State.pinnedDoors, SystemTypes__Enum::Security);
    }

    bool ReadState(bool& anyOpen) {
        Group group;
        if (!Resolve(group)) return false;
        auto open = group.State();
        anyOpen = std::any_of(open.begin(), open.end(), [](bool value) { return value; });
        return true;
    }

    bool CanControl(bool notify) {
        if (State.PanicMode) return false;
        if (!IsHost()) {
            if (notify) Toasts::AddToast("Security doors", "You must be the host to control Security doors.");
            return false;
        }
        Group group;
        if (Resolve(group)) return true;
        if (notify) Unavailable();
        return false;
    }

    bool SetOpen(bool open) {
        if (!CanControl(true)) return false;
        if (open && (Contains(State.pinnedDoors, SystemTypes__Enum::Security) ||
                     Contains(State.pinnedDoors, SystemTypes__Enum::Kitchen))) {
            Toasts::AddToast("Security doors", "Unpin Security and Kitchen before opening Security.");
            return false;
        }
        Group group;
        if (!Resolve(group)) return false;
        if (!group.Apply(AllMask, open)) {
            Unavailable();
            return false;
        }
        timer.Reset();
        return true;
    }

    void CloseKitchen() {
        if (State.PanicMode || !IsHost()) return;
        Group group;
        if (Resolve(group)) group.Apply(KitchenMask, false);
    }

    void UpdatePins() {
        const auto room = SystemTypes__Enum::Security;
        if (State.PanicMode || !IsGroup(room) || !IsHost() || !IsInGame() || State.InMeeting || State.InExileUI) {
            timer.Reset();
            return;
        }
        const auto mode = Contains(State.pinnedDoors, room) ? PinMode::Hard :
            Contains(State.softPinnedDoors, room) ? PinMode::Soft : PinMode::None;
        if (mode == PinMode::None) { timer.Reset(); return; }
        Group group;
        if (!Resolve(group)) { timer.Reset(); return; }
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        const auto close = timer.Update(mode, group.State(), now);
        if (close) group.Apply(close, false);
    }

    void Reset() { timer.Reset(); }
}
