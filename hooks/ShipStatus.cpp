#include "pch-il2cpp.h"
#include "_hooks.h"
#include "state.hpp"
#include "toasts.hpp"
#include "console.hpp"
#include "logger.h"
#include "utility.h"
#include "replay.hpp"
#include "profiler.h"
#include "game.h"
#include "security_doors.h"
#include "polus_decon.h"

#include <cstring>

static bool IsHardPinnedDoorRoom(SystemTypes__Enum room) {
    return !State.PanicMode &&
        std::find(State.pinnedDoors.begin(), State.pinnedDoors.end(), room) != State.pinnedDoors.end();
}

static bool IsPolusOrAirship() {
    return State.mapType == Settings::MapType::Airship || State.mapType == Settings::MapType::Pb;
}

static bool DoorClassMatches(Il2CppClass* klass, const char* name) {
    for (auto k = klass; k != nullptr; k = k->parent) {
        if (k->name != nullptr && std::strcmp(k->name, name) == 0)
            return true;
    }
    return false;
}

static OpenableDoor* FindDoorFromDoorsSystemAmount(ShipStatus* shipStatus, uint8_t amount) {
    if (shipStatus == nullptr || shipStatus->fields.AllDoors == nullptr)
        return nullptr;

    const uint8_t requestedDoorId = static_cast<uint8_t>(amount & ~64);
    for (auto door : il2cpp::Array(shipStatus->fields.AllDoors)) {
        if (door == nullptr)
            continue;

        const uint8_t doorId = static_cast<uint8_t>(door->fields.Id & ~64);
        if (doorId == requestedDoorId)
            return door;
    }

    return nullptr;
}

static void ForceDoorClosedLocally(OpenableDoor* door) {
    if (door == nullptr || door->klass == nullptr)
        return;

    if (DoorClassMatches(door->klass, "PlainDoor")) {
        app::PlainDoor_SetDoorway(reinterpret_cast<PlainDoor*>(door), false, nullptr);
    }
    else if (DoorClassMatches(door->klass, "MushroomWallDoor")) {
        app::MushroomWallDoor_SetDoorway(reinterpret_cast<MushroomWallDoor*>(door), false, nullptr);
    }
}

static bool ShouldSuppressHardPinnedDoorOpen(
    ShipStatus* shipStatus,
    SystemTypes__Enum systemType,
    uint8_t amount,
    OpenableDoor** doorOut,
    SystemTypes__Enum* roomOut) {

    // In Polus/Airship door systems, bit 0x40 is the existing open marker used
    // elsewhere in this mod (door->Id | 64 opens, door->Id & ~64 closes).
    if (State.PanicMode || systemType != SystemTypes__Enum::Doors || State.pinnedDoors.empty() || !IsPolusOrAirship())
        return false;

    if ((amount & 64) == 0)
        return false; // Already a close/no-open update; let vanilla handle it.

    OpenableDoor* door = FindDoorFromDoorsSystemAmount(shipStatus, amount);
    if (door == nullptr || PolusDecon::IsPhysicalDoor(door) || !IsHardPinnedDoorRoom(door->fields.Room))
        return false;

    if (doorOut != nullptr)
        *doorOut = door;
    if (roomOut != nullptr)
        *roomOut = door->fields.Room;
    return true;
}

float dShipStatus_CalculateLightRadius(ShipStatus* __this, NetworkedPlayerInfo* player, MethodInfo* method) {
    if (State.ShowHookLogs) Log.HookDebug("Hook dShipStatus_CalculateLightRadius executed", false);
    if (IsHost() && State.TaskSpeedrun && State.GameLoaded && State.mapType != Settings::MapType::Airship)
        State.SpeedrunTimer += Time_get_deltaTime(NULL);
    CosmeticsCache_PopulateFromPlayers((CosmeticsCache*)__this->fields._CosmeticsCache_k__BackingField, NULL);
    switch (__this->fields.Type) {
    case ShipStatus_MapType__Enum::Ship:
        if (State.mapType != Settings::MapType::Airship) State.mapType = Settings::MapType::Ship;
        break;
    case ShipStatus_MapType__Enum::Hq:
        State.mapType = Settings::MapType::Hq;
        break;
    case ShipStatus_MapType__Enum::Pb:
        State.mapType = Settings::MapType::Pb;
        break;
    case ShipStatus_MapType__Enum::Fungle:
        State.mapType = Settings::MapType::Fungle;
        break;
    }

    if (!State.PanicMode && State.MaxVision)
        return 420.F;
    else
        return ShipStatus_CalculateLightRadius(__this, player, method);
}

void dShipStatus_OnEnable(ShipStatus* __this, MethodInfo* method) {
    if (State.ShowHookLogs) Log.HookDebug("Hook dShipStatus_OnEnable executed", false);
    try {
        State.BlinkPlayersTab = false;
        State.SpamZiplineEveryone = false;

        Replay::Reset();

        State.MatchStart = std::chrono::system_clock::now();
        State.MatchCurrent = State.MatchStart;

        State.selectedDoors.clear();
        State.mapDoors.clear();
        State.pinnedDoors.clear();
        State.softPinnedDoors.clear();
        State.doorOpenTimes.clear();
        State.pinnedDoorLastCheck.clear();

        il2cpp::Array allDoors = __this->fields.AllDoors;

        for (auto door : allDoors) {
            if (std::find(State.mapDoors.begin(), State.mapDoors.end(), door->fields.Room) == State.mapDoors.end())
                State.mapDoors.push_back(door->fields.Room);
        }

        std::sort(State.mapDoors.begin(), State.mapDoors.end());

        if (State.AutoFakeRole) {
            if (!State.SafeMode) State.rpcQueue.push(new RpcSetRole(*Game::pLocalPlayer, (RoleTypes__Enum)State.FakeRole));
        }

        auto transform = Component_get_transform((Component_1*)__this, NULL);
        auto localScale = transform == NULL ? Vector3(0.f, 0.f, 0.f) : Transform_get_localScale(transform, NULL);

        if (State.mapType == Settings::MapType::Ship) {
            State.FlipSkeld = localScale.x < 0;
            // By default, the x coordinate of the local scale of the Skeld is 1.2,
            // whereas it is -1.2 for Dleks.
            // This allows us to flip stuff such as the radar,
            // even when we aren't hosting and the host plays with Dleks.
        }
        else State.FlipSkeld = false;

        //if (!State.mapDoors.empty() && Constants_1_ShouldFlipSkeld(NULL))
            //State.FlipSkeld = true; fix later

//          if (State.FlipSkeld && IsHost() && GameOptions().GetByte(app::ByteOptionNames__Enum::MapId) != 3) {
//              GameOptions().SetByte(app::ByteOptionNames__Enum::MapId, 3);
            /*if (GameOptionsManager_get_Instance && GameOptionsManager_get_CurrentGameOptions && GameOptionsManager_set_GameHostOptions
                && GameManager_get_Instance && GameManager_get_LogicOptions && LogicOptions_SyncOptions) {
                auto gameOptionsManager = GameOptionsManager_get_Instance(NULL);
                GameManager* gameManager = GameManager_get_Instance(NULL);
                if (gameOptionsManager != nullptr) {
                    auto currentOptions = GameOptionsManager_get_CurrentGameOptions(gameOptionsManager, NULL);
                    if (currentOptions != nullptr)
                        GameOptionsManager_set_GameHostOptions(gameOptionsManager, currentOptions, NULL);
                }
                if (gameManager != nullptr) {
                    auto logicOptions = GameManager_get_LogicOptions(gameManager, NULL);
                    if (logicOptions != nullptr)
                        LogicOptions_SyncOptions(logicOptions, NULL);
                }
            }*/
    }
    catch (...) {
        LOG_ERROR("Exception occurred in ShipStatus_OnEnable (ShipStatus)");
    }
    ShipStatus_OnEnable(__this, method);
}

static bool IsSabotageTriggerAmount(SystemTypes__Enum systemType, int32_t amount) {
    switch (systemType) {
    case SystemTypes__Enum::Electrical: return amount >= 5;
    case SystemTypes__Enum::MushroomMixupSabotage: return amount >= 1;
    default: return amount >= 128;
    }
}

void dShipStatus_RpcUpdateSystem(ShipStatus* __this, SystemTypes__Enum systemType, int32_t amount, MethodInfo* method) {
    if (State.ShowHookLogs) Log.HookDebug("Hook dShipStatus_RpcUpdateSystem executed", false);

    ShipStatus_RpcUpdateSystem(__this, systemType, amount, method);
}

void dShipStatus_RpcCloseDoorsOfType(ShipStatus* __this, SystemTypes__Enum type, MethodInfo* method) {
    if (State.ShowHookLogs) Log.HookDebug("Hook dShipStatus_RpcCloseDoorsOfType executed", false);
    ShipStatus_RpcCloseDoorsOfType(__this, type, method);
}

void dShipStatus_HandleRpc(ShipStatus* __this, uint8_t callId, MessageReader* reader, MethodInfo* method) {
    if (State.ShowHookLogs) Log.HookDebug("Hook dShipStatus_HandleRpc executed", false);

    if (callId == 67) { // haha SpiritGuideMessage is 67 OMG SIX SEVEN
        ShipStatus_HandleRpc(__this, callId, reader, method);
        return;
    }

    if (callId != 27 && callId != 35) return;
    int32_t pos = reader->fields._position, head = reader->fields.readHead;
    auto systemType = (SystemTypes__Enum)MessageReader_ReadByte(reader, NULL);

    if (!State.PanicMode && !IsHost() && State.AntiExploit_UnauthorizedSabotages && systemType != SystemTypes__Enum::Ventilation &&
        callId != (uint8_t)RpcCalls__Enum::SpiritGuideMessage)
        return;
    // VentilationSystem is handled separately

    reader->fields._position = pos;
    reader->fields.readHead = head;
    if (systemType == SystemTypes__Enum::Ventilation ||
        (callId == 35 && systemType == SystemTypes__Enum::Security) ||
        systemType == SystemTypes__Enum::Decontamination ||
        systemType == SystemTypes__Enum::Decontamination2 ||
        systemType == SystemTypes__Enum::Decontamination3 ||
        (callId == 35 && systemType == SystemTypes__Enum::MedBay))
        return ShipStatus_HandleRpc(__this, callId, reader, method);
    if (!State.PanicMode && State.DisableSabotages && IsHost()) return;
    if (!State.PanicMode && callId == (uint8_t)RpcCalls__Enum::CloseDoorsOfType &&
        State.DisabledSabotageTypes.count((int)SystemTypes__Enum::Doors))
        return;
    if (!State.PanicMode && State.DisabledSabotageTypes.count((int)systemType))
        return;

    ShipStatus_HandleRpc(__this, callId, reader, method);
}

bool DetectCheatSabotageResult(PlayerControl* player, bool result) {
    if (State.Enable_SMAC && State.SMAC_CheckSabotage) SMAC_OnCheatDetected(player, "Bad Sabotage");
    return result;
}

bool DetectCheatSabotage(SystemTypes__Enum systemType, PlayerControl* player, uint8_t amount) {
    Settings::MapType mapId = State.mapType;
    if (systemType == SystemTypes__Enum::Sabotage && PlayerIsImpostor(GetPlayerData(player)))
        return false;
    else if (systemType == SystemTypes__Enum::LifeSupp &&
        (mapId == Settings::MapType::Ship || mapId == Settings::MapType::Hq) && (amount == 64 || amount == 65))
        return false;
    // Only Skeld and Mira have oxygen sabotage
    else if (systemType == SystemTypes__Enum::Comms) {
        if (amount == 0 && mapId != Settings::MapType::Hq && mapId != Settings::MapType::Fungle) return false;
        if ((amount == 64 || amount == 65 || amount == 32 || amount == 33 || amount == 16 || amount == 17)
            && (mapId == Settings::MapType::Hq || mapId == Settings::MapType::Fungle)) return false;
    }
    else if (systemType == SystemTypes__Enum::Electrical) {
        if (mapId != Settings::MapType::Fungle && amount < 5) return false;
        else if (amount >= 5 && !(State.DisableSabotages && IsHost())) {
            return DetectCheatSabotageResult(player, false);
        }
    }
    else if (systemType == SystemTypes__Enum::Laboratory &&
        mapId == Settings::MapType::Pb && (amount == 64 || amount == 65 || amount == 32 || amount == 33))
        return false;
    else if (systemType == SystemTypes__Enum::Reactor &&
        mapId != Settings::MapType::Pb && mapId != Settings::MapType::Airship && (amount == 64 || amount == 65 || amount == 32 || amount == 33))
        return false;
    else if (systemType == SystemTypes__Enum::HeliSabotage &&
        mapId == Settings::MapType::Airship && (amount == 64 || amount == 65 || amount == 16 || amount == 17 || amount == 32 || amount == 33))
        return false;
    else if (systemType == SystemTypes__Enum::MushroomMixupSabotage) {
        if (mapId == Settings::MapType::Fungle && !(State.DisableSabotages && IsHost())) {
            return DetectCheatSabotageResult(player, false);
        }
    }
    else if (State.InMeeting) {
        if (!(State.DisableSabotages && IsHost())) {
            return DetectCheatSabotageResult(player, false);
        }
    }
    return DetectCheatSabotageResult(player, true);
    return false;
}

void dShipStatus_UpdateSystem(ShipStatus* __this, SystemTypes__Enum systemType, PlayerControl* player, uint8_t amount, MethodInfo* method) {
    if (State.ShowHookLogs) Log.HookDebug("Hook dShipStatus_UpdateSystem executed", false);
    // Reject activation before vanilla changes a hard-pinned chamber's state.
    if (PolusDecon::IsHardPinned(systemType)) return;
    if (systemType == SystemTypes__Enum::Doors && (amount & 64) != 0) {
        SystemTypes__Enum chamber{};
        if (PolusDecon::RoomForDoor(FindDoorFromDoorsSystemAmount(__this, amount), chamber) &&
            PolusDecon::IsHardPinned(chamber)) {
            PolusDecon::SetOpen(chamber, false);
            return;
        }
    }
    // Security owns only Kitchen door 9. Reject its open before vanilla applies
    // it, and publish just that door's state instead of closing the whole Kitchen.
    if (systemType == SystemTypes__Enum::Doors && (amount & 64) != 0 &&
        SecurityDoors::IsHardPinnedKitchen(FindDoorFromDoorsSystemAmount(__this, amount))) {
        SecurityDoors::CloseKitchen();
        return;
    }
    LOG_DEBUG(std::format("SystemType {} updated with amount {}", (std::string)TranslateSystemTypes(systemType), amount).c_str());
    OpenableDoor* hardPinnedDoor = nullptr;
    SystemTypes__Enum hardPinnedRoom = SystemTypes__Enum::Hallway;
    if (ShouldSuppressHardPinnedDoorOpen(__this, systemType, amount, &hardPinnedDoor, &hardPinnedRoom)) {
        // Critical fix: reject the open before vanilla applies it. The old path
        // opened first and only sent a close afterward, so remote clients could
        // render a short open window and squeeze through.
        ForceDoorClosedLocally(hardPinnedDoor);

        // If this client is host, also broadcast a close immediately so clients
        // that do not have this receiver-side guard collapse back to closed as
        // soon as the host sees the bad open attempt.
        if (IsHost())
            app::ShipStatus_RpcCloseDoorsOfType(__this, hardPinnedRoom, nullptr);
        return;
    }

    if (systemType == SystemTypes__Enum::Ventilation ||
        systemType == SystemTypes__Enum::Security ||
        systemType == SystemTypes__Enum::Decontamination ||
        systemType == SystemTypes__Enum::Decontamination2 ||
        systemType == SystemTypes__Enum::Decontamination3 ||
        systemType == SystemTypes__Enum::MedBay)
        return ShipStatus_UpdateSystem(__this, systemType, player, amount, method);
    if (!State.PanicMode && State.DisableSabotages && IsHost()) return;

    if (!State.PanicMode && IsHost() && IsSabotageTriggerAmount(systemType, amount)) {
        if (State.DisabledSabotageTypes.count((int)systemType)) {
            RepairSabotage(player);
            return;
        }
        if (systemType == SystemTypes__Enum::Doors &&
            State.DisabledSabotageTypes.count((int)SystemTypes__Enum::Doors))
            return;
    }
    if (player != nullptr && systemType != SystemTypes__Enum::Electrical) {
        auto evtPlayer = GetEventPlayerControl(player);
        if (evtPlayer.has_value()) {
            bool isSabotage = amount >= 128;
            SABOTAGE_ACTIONS action = isSabotage ? SABOTAGE_ACTIONS::SABOTAGE_CALL : SABOTAGE_ACTIONS::SABOTAGE_FIX;
            synchronized(Replay::replayEventMutex) {
                auto source = evtPlayer.value();
                State.liveConsoleEvents.emplace_back(std::make_unique<SabotageEvent>(source, systemType, action));

                if (State.ShowConsoleEventsAsToasts &&
                    ConsoleGui::IsEventFiltered(EVENT_TYPES::EVENT_SABOTAGE) &&
                    ConsoleGui::IsPlayerFiltered(player->fields.PlayerId)) {
                    std::string toastContent = std::format("{} ({}) {} {}!",
                        source.playerName, GetColorName(source.colorId),
                        isSabotage ? "sabotaged" : "repaired",
                        TranslateSystemTypes(systemType));
                    Toasts::AddToast(isSabotage ? "Player Sabotaged" : "Player Fixed Sabotage", toastContent,
                        isSabotage ? ImVec4(1.f, 0.f, 0.f, 1.f) : ImVec4(0.f, 1.f, 0.f, 1.f));
                }
            }
        }
    }

    ShipStatus_UpdateSystem(__this, systemType, player, amount, method);
}

void dShipStatus_AddTasksFromList(ShipStatus* __this, int32_t* start, int32_t count, void* tasks, void* usedTaskTypes, List_1_NormalPlayerTask_* unusedTasks, MethodInfo* method) {
    if (State.DisableMedbayScan || !State.DisabledTaskTypes.empty()) {
        il2cpp::List<List_1_NormalPlayerTask_> taskList = unusedTasks;
        for (int i = (int)taskList.size() - 1; i >= 0; i--) {
            if ((int)taskList.size() <= count) break; 
            auto taskType = taskList[i]->fields._.TaskType;
            bool remove = (State.DisableMedbayScan && taskType == TaskTypes__Enum::SubmitScan)
                || State.DisabledTaskTypes.count((int)taskType);
            if (remove) taskList.erase(i);
        }
        unusedTasks = taskList.get();
    }
    ShipStatus_AddTasksFromList(__this, start, count, tasks, usedTaskTypes, unusedTasks, method);
}
