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

namespace KitchenEast {
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

        struct Group {
            PlainDoor* kitchen = nullptr;
            Il2CppObject* normalSystem = nullptr;
            FieldInfo* normalDirty = nullptr;

            void Apply(bool open) const {
                app::PlainDoor_SetDoorway(kitchen, open, nullptr);
                bool dirty = true;
                il2cpp_field_set_value(normalSystem, normalDirty, &dirty);
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
            if (!group.kitchen || !group.normalSystem ||
                !ClassMatches(group.normalSystem->klass, "DoorsSystemType")) return false;
            group.normalDirty = Field(group.normalSystem, "<IsDirty>k__BackingField");
            return group.normalDirty != nullptr;
        }

        void Unavailable() {
            Toasts::AddToast("KitchenEast", "KitchenEast controls are unavailable for this Airship layout.");
        }
    }

    // Security is a UI-only selection key; the actual target is Kitchen door 9.
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
        anyOpen = group.kitchen->fields.Open;
        return true;
    }

    bool CanControl(bool notify) {
        if (State.PanicMode) return false;
        if (!IsHost()) {
            if (notify) Toasts::AddToast("KitchenEast", "You must be the host to control KitchenEast.");
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
            Toasts::AddToast("KitchenEast", "Unpin KitchenEast and Kitchen before opening KitchenEast.");
            return false;
        }
        Group group;
        if (!Resolve(group)) return false;
        group.Apply(open);
        timer.Reset();
        return true;
    }

    void CloseKitchen() {
        if (State.PanicMode || !IsHost()) return;
        Group group;
        if (Resolve(group)) group.Apply(false);
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
        const auto close = timer.Update(mode, group.kitchen->fields.Open, now);
        if (close) group.Apply(false);
    }

    void Reset() { timer.Reset(); }
}
