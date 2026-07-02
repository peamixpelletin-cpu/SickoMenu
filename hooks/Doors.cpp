#include "pch-il2cpp.h"
#include "_hooks.h"
#include "state.hpp"
#include "_rpc.h"

void dPlainDoor_SetDoorway(PlainDoor* __this, bool open, MethodInfo* method) {
	if (State.ShowHookLogs) LOG_DEBUG("Hook dPlainDoor_SetDoorway executed");
	if (open && (std::find(State.pinnedDoors.begin(), State.pinnedDoors.end(), __this->fields._.Room) != State.pinnedDoors.end())) {
		// Close locally for instant feedback before the RPC round-trips back
		app::PlainDoor_SetDoorway(__this, false, method);
		// Also send the system update so the door's networked state is closed immediately
		ShipStatus_RpcUpdateSystem(*Game::pShipStatus, SystemTypes__Enum::Doors, (uint8_t)(__this->fields._.Id & ~64), NULL);
		ShipStatus_RpcCloseDoorsOfType(*Game::pShipStatus, __this->fields._.Room, NULL);
		return;
	}
	app::PlainDoor_SetDoorway(__this, open, method);
}

void dMushroomWallDoor_SetDoorway(MushroomWallDoor* __this, bool open, MethodInfo* method) {
	if (State.ShowHookLogs) LOG_DEBUG("Hook dMushroomWallDoor_SetDoorway executed");
	if (open && (std::find(State.pinnedDoors.begin(), State.pinnedDoors.end(), __this->fields._.Room) != State.pinnedDoors.end())) {
		// Close locally for instant feedback before the RPC round-trips back
		app::MushroomWallDoor_SetDoorway(__this, false, method);
		// Also send the system update so the door's networked state is closed immediately
		ShipStatus_RpcUpdateSystem(*Game::pShipStatus, SystemTypes__Enum::Doors, (uint8_t)(__this->fields._.Id & ~64), NULL);
		ShipStatus_RpcCloseDoorsOfType(*Game::pShipStatus, __this->fields._.Room, NULL);
		return;
	}
	app::MushroomWallDoor_SetDoorway(__this, open, method);
}

bool dAutoOpenDoor_DoUpdate(AutoOpenDoor* __this, float dt, MethodInfo* method) {
	if (State.ShowHookLogs) LOG_DEBUG("Hook dAutoOpenDoor_DoUpdate executed");
	/*if (__this->fields._.Open && (std::find(State.pinnedDoors.begin(), State.pinnedDoors.end(), __this->fields._._.Room) != State.pinnedDoors.end()) && __this->fields.ClosedTimer < 1.5f &&
		__this->fields._._.Room != SystemTypes__Enum::Decontamination && __this->fields._._.Room != SystemTypes__Enum::Decontamination2
		&& __this->fields._._.Room != SystemTypes__Enum::Decontamination3) {
		ShipStatus_RpcCloseDoorsOfType(*Game::pShipStatus, __this->fields._._.Room, NULL);
	}*/
	return app::AutoOpenDoor_DoUpdate(__this, dt, method);
}