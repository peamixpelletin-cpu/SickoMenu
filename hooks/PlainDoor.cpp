#include "pch-il2cpp.h"
#include "_hooks.h"
#include "state.hpp"
#include <iostream>

using namespace std::string_view_literals;


static bool IsHardPinnedDoor(OpenableDoor* door) {
    if (door == nullptr || State.PanicMode)
        return false;

    const bool isAirshipOrPolus = State.mapType == Settings::MapType::Airship || State.mapType == Settings::MapType::Pb;
    return isAirshipOrPolus &&
        std::find(State.pinnedDoors.begin(), State.pinnedDoors.end(), door->fields.Room) != State.pinnedDoors.end();
}

static void CloseDoorLocally(OpenableDoor* door) {
    if (door == nullptr || door->klass == nullptr)
        return;

    if ("PlainDoor"sv == door->klass->name || (door->klass->parent != nullptr && "PlainDoor"sv == door->klass->parent->name))
        app::PlainDoor_SetDoorway(reinterpret_cast<PlainDoor*>(door), false, nullptr);
    else if ("MushroomWallDoor"sv == door->klass->name)
        app::MushroomWallDoor_SetDoorway(reinterpret_cast<MushroomWallDoor*>(door), false, nullptr);
}

static bool OpenDoor(OpenableDoor* door) {
    if (door == nullptr)
        return false;

    if (IsHardPinnedDoor(door)) {
        // Auto-open minigames call app::SetDoorway directly and used to bypass
        // dPlainDoor_SetDoorway/dMushroomWallDoor_SetDoorway. Treat hard-pinned
        // doors as handled, but never emit the open RPC.
        CloseDoorLocally(door);
        State.rpcQueue.push(new RpcCloseDoorsOfType(door->fields.Room, false));
        return true;
    }

    if ("PlainDoor"sv == door->klass->name || (door->klass->parent != nullptr && "PlainDoor"sv == door->klass->parent->name)) {
        app::PlainDoor_SetDoorway(reinterpret_cast<PlainDoor*>(door), true, {});
    }
    else if ("MushroomWallDoor"sv == door->klass->name) {
        app::MushroomWallDoor_SetDoorway(reinterpret_cast<MushroomWallDoor*>(door), true, {});
    }
    else {
        return false;
    }
    State.rpcQueue.push(new RpcUpdateSystem(SystemTypes__Enum::Doors, door->fields.Id | 64));
    return true;
}

void dDoorBreakerGame_Start(DoorBreakerGame* __this, MethodInfo* method) {
    if (State.ShowHookLogs) LOG_DEBUG("Hook dDoorBreakerGame_Start executed");
    if (!State.PanicMode && State.AutoOpenDoors) {
        if (OpenDoor(__this->fields.MyDoor)) {
            Minigame_Close((Minigame*)__this, {});
            return;
        }
    }
    DoorBreakerGame_Start(__this, method);
}

void dDoorCardSwipeGame_Begin(DoorCardSwipeGame* __this, PlayerTask* playerTask, MethodInfo* method) {
    if (State.ShowHookLogs) LOG_DEBUG("Hook dDoorCardSwipeGame_Begin executed");
    if (!State.PanicMode && State.AutoOpenDoors) {
        if (OpenDoor(__this->fields.MyDoor)) {
            Minigame_Close((Minigame*)__this, {});
            return;
        }
    }
    DoorCardSwipeGame_Begin(__this, playerTask, method);
}

void dMushroomDoorSabotageMinigame_Begin(MushroomDoorSabotageMinigame* __this, PlayerTask* task, MethodInfo* method) {
    if (State.ShowHookLogs) LOG_DEBUG("Hook dMushroomDoorSabotageMinigame_Begin executed");
    if (!State.PanicMode) {
        if (State.AutoOpenDoors) {
            if (OpenDoor(__this->fields.myDoor)) {
                Minigame_Close((Minigame*)__this, {});
                return;
            }
        }
    }
    MushroomDoorSabotageMinigame_Begin(__this, task, method);
}