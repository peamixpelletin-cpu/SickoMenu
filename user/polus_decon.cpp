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
            Il2CppObject* system = nullptr;
            FieldInfo* state = nullptr;
            FieldInfo* time = nullptr;
            FieldInfo* dirty = nullptr;
            FieldInfo* openTime = nullptr;
            const MethodInfo* updateDoors = nullptr;
            Vector3 upper{}, lower{};
            uint32_t StateValue() const {
                uint32_t value = 0;
                il2cpp_field_get_value(system, state, &value);
                return value;
            }
            bool Apply(uint32_t value, float duration) const {
                il2cpp_field_set_value(system, state, &value);
                il2cpp_field_set_value(system, time, &duration);
                Il2CppException* exception = nullptr;
                il2cpp_runtime_invoke(updateDoors, system, nullptr, &exception);
                bool changed = true;
                // Publish the native state and timer, not regular-door RPC IDs.
                il2cpp_field_set_value(system, dirty, &changed);
                return exception == nullptr;
            }
        };
        bool Resolve(SystemTypes__Enum room, Chamber& chamber) {
            chamber.system = System(room);
            if (!chamber.system) return false;
            auto klass = chamber.system->klass;
            chamber.state = il2cpp_class_get_field_from_name(klass, "<CurState>k__BackingField");
            chamber.time = il2cpp_class_get_field_from_name(klass, "timer");
            chamber.dirty = il2cpp_class_get_field_from_name(klass, "<IsDirty>k__BackingField");
            chamber.openTime = il2cpp_class_get_field_from_name(klass, "DoorOpenTime");
            chamber.updateDoors = il2cpp_class_get_method_from_name(klass, "UpdateDoorsViaState", 0);
            return chamber.state && chamber.time && chamber.dirty && chamber.openTime && chamber.updateDoors &&
                Position(Reference(chamber.system, "UpperDoor"), chamber.upper) &&
                Position(Reference(chamber.system, "LowerDoor"), chamber.lower);
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
        // DeconSystem's SomeKindaDoor references are absent from ShipStatus.AllDoors.
        // Resolve lazily, after the game's systems have finished initializing.
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
        anyOpen = IsOpenState(chamber.StateValue());
        return true;
    }
    bool CanControl(SystemTypes__Enum room, bool notify) {
        if (State.PanicMode) return false;
        if (!IsHost()) {
            if (notify) Toasts::AddToast("Decontamination", "You must be the host to control decontamination.");
            return false;
        }
        Chamber chamber;
        if (Resolve(room, chamber)) return true;
        if (notify) Unavailable();
        return false;
    }
    bool IsHardPinned(SystemTypes__Enum room) {
        return !State.PanicMode && IsHost() && IsGroup(room) && Contains(State.pinnedDoors, room);
    }
    bool SetOpen(SystemTypes__Enum room, bool open) {
        if (!CanControl(room, true) || (open && IsHardPinned(room))) return false;
        Chamber chamber;
        if (!Resolve(room, chamber)) return false;
        uint32_t value = 0;
        float duration = 0.f;
        if (open) {
            if (!Game::pLocalPlayer || !*Game::pLocalPlayer) return false;
            const auto position = GetTrueAdjustedPosition(*Game::pLocalPlayer);
            auto distance = [&](Vector3 door) {
                const float x = position.x - door.x, y = position.y - door.y;
                return x * x + y * y;
            };
            value = EntryState(distance(chamber.upper) <= distance(chamber.lower));
            il2cpp_field_get_value(chamber.system, chamber.openTime, &duration);
        }
        Reset(room);
        if (!chamber.Apply(value, duration)) { Unavailable(); return false; }
        return true;
    }
    void UpdatePins() {
        if (State.PanicMode || State.mapType != Settings::MapType::Pb || !IsHost() ||
            !IsInGame() || State.InMeeting || State.InExileUI) { Reset(); return; }
        const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        for (size_t i = 0; i < rooms.size(); ++i) {
            const bool hard = Contains(State.pinnedDoors, rooms[i]);
            const bool soft = Contains(State.softPinnedDoors, rooms[i]);
            if (!hard && !soft) { timers[i].Reset(); continue; }
            Chamber chamber;
            if (!Resolve(rooms[i], chamber)) { timers[i].Reset(); continue; }
            if (timers[i].Update(hard, soft, chamber.StateValue(), now)) {
                chamber.Apply(0, 0.f);
                timers[i].Reset();
            }
        }
    }
    void Reset(SystemTypes__Enum room) {
        const auto found = std::find(rooms.begin(), rooms.end(), room);
        if (found != rooms.end()) timers[static_cast<size_t>(found - rooms.begin())].Reset();
    }
    void Reset() { for (auto& timer : timers) timer.Reset(); }
}
