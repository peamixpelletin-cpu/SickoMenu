#include "pch-il2cpp.h"
#include "_rpc.h"
#include "game.h"
#include "state.hpp"

using namespace std::string_view_literals;

RpcCloseDoorsOfType::RpcCloseDoorsOfType(SystemTypes__Enum selectedSystem, bool pinDoor)
{
	this->selectedSystem = selectedSystem;
	this->pinDoor = pinDoor;
}

void RpcCloseDoorsOfType::Process()
{
	if (State.PanicMode || !Game::pShipStatus || !*Game::pShipStatus) return;
	app::ShipStatus_RpcCloseDoorsOfType(*Game::pShipStatus, this->selectedSystem, NULL);
    if (this->pinDoor && std::find(State.pinnedDoors.begin(), State.pinnedDoors.end(), selectedSystem) == State.pinnedDoors.end()) {
        State.softPinnedDoors.erase(std::remove(State.softPinnedDoors.begin(), State.softPinnedDoors.end(), selectedSystem), State.softPinnedDoors.end());
        State.pinnedDoors.push_back(selectedSystem);
    }
}

RpcOpenDoorsOfType::RpcOpenDoorsOfType(SystemTypes__Enum selectedSystem)
{
	this->selectedSystem = selectedSystem;
}

void RpcOpenDoorsOfType::Process()
{
    if (State.PanicMode || !Game::pShipStatus || !*Game::pShipStatus || !(*Game::pShipStatus)->fields.AllDoors) return;
    if (std::find(State.pinnedDoors.begin(), State.pinnedDoors.end(), selectedSystem) != State.pinnedDoors.end()) return;
	for (auto door : il2cpp::Array((*Game::pShipStatus)->fields.AllDoors))
	{
		if (door && door->klass && door->fields.Room == selectedSystem)
		{
			app::ShipStatus_RpcUpdateSystem(*Game::pShipStatus, SystemTypes__Enum::Doors, (uint8_t)(door->fields.Id | 64), NULL);
			if ("PlainDoor"sv == door->klass->name || (door->klass->parent && "PlainDoor"sv == door->klass->parent->name))
                app::PlainDoor_SetDoorway(reinterpret_cast<PlainDoor*>(door), true, {});
			else if ("MushroomWallDoor"sv == door->klass->name) app::MushroomWallDoor_SetDoorway(reinterpret_cast<MushroomWallDoor*>(door), true, {});
		}
	}
}
