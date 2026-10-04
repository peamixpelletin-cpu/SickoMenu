#include "pch-il2cpp.h"
#include "doors_tab.h"
#include "game.h"
#include "gui-helpers.hpp"
#include "imgui/imgui.h"
#include "state.hpp"
#include "utility.h"
#include "gui-helpers.hpp"

using namespace std::string_view_literals;

namespace DoorsTab {
    static bool IsDecontamination(SystemTypes__Enum room) {
        return room == SystemTypes__Enum::Decontamination || room == SystemTypes__Enum::Decontamination2 ||
            room == SystemTypes__Enum::Decontamination3;
    }

    static const char* GetDoorLabel(SystemTypes__Enum room) {
        if (State.mapType != Settings::MapType::Pb || !IsDecontamination(room) ||
            !Game::pShipStatus || !*Game::pShipStatus || !(*Game::pShipStatus)->fields.AllDoors)
            return TranslateSystemTypes(room);

        // Use the chamber's average world Y, not an assumed system-ID ordering.
        // Each chamber can contain multiple physical doors with the same Room.
        float roomY = 0.f, otherY = 0.f;
        int roomCount = 0, otherCount = 0;
        for (auto door : il2cpp::Array((*Game::pShipStatus)->fields.AllDoors)) {
            if (!door || !IsDecontamination(door->fields.Room)) continue;
            auto transform = app::Component_get_transform(reinterpret_cast<Component_1*>(door), nullptr);
            if (!transform) continue;
            const float y = app::Transform_get_position(transform, nullptr).y;
            if (door->fields.Room == room) {
                roomY += y;
                ++roomCount;
            }
            else {
                otherY += y;
                ++otherCount;
            }
        }
        if (roomCount == 0 || otherCount == 0) return TranslateSystemTypes(room);
        return roomY / roomCount > otherY / otherCount ?
            "Decontamination (Upper)" : "Decontamination (Lower)";
    }

    static void Unpin(SystemTypes__Enum room) {
        State.pinnedDoors.erase(std::remove(State.pinnedDoors.begin(), State.pinnedDoors.end(), room), State.pinnedDoors.end());
        State.softPinnedDoors.erase(std::remove(State.softPinnedDoors.begin(), State.softPinnedDoors.end(), room), State.softPinnedDoors.end());
        State.doorOpenTimes.clear();
        State.pinnedDoorLastCheck.erase(room);
    }

    static void Pin(SystemTypes__Enum room, bool soft) {
        Unpin(room);
        (soft ? State.softPinnedDoors : State.pinnedDoors).push_back(room);
        State.rpcQueue.push(new RpcCloseDoorsOfType(room, false));
    }

	void Render() {
		if (IsInGame() && !State.mapDoors.empty()) {
			ImGui::SameLine(100 * State.dpiScale);
			ImGui::BeginChild("doors#list", ImVec2(200, 0) * State.dpiScale, true, ImGuiWindowFlags_NoBackground);
			bool shouldEndListBox = ImGui::ListBoxHeader("###doors#list", ImVec2(200, 150) * State.dpiScale);
			if (shouldEndListBox) {
				for (auto systemType : State.mapDoors) {
					bool isOpen;
					auto openableDoor = GetOpenableDoorByRoom(systemType);
					if (!openableDoor || !openableDoor->klass) continue;
					if ((openableDoor->klass->parent && "PlainDoor"sv == openableDoor->klass->parent->name)
						|| "PlainDoor"sv == openableDoor->klass->name) {
						isOpen = reinterpret_cast<PlainDoor*>(openableDoor)->fields.Open;
					}
					else if ("MushroomWallDoor"sv == openableDoor->klass->name) {
						isOpen = reinterpret_cast<MushroomWallDoor*>(openableDoor)->fields.open;
					}
					else {
						continue;
					}
					bool isPinned = std::find(State.pinnedDoors.begin(), State.pinnedDoors.end(), systemType) != State.pinnedDoors.end();

					bool isSoftPinned = std::find(State.softPinnedDoors.begin(), State.softPinnedDoors.end(), systemType) != State.softPinnedDoors.end();
					bool isSelected = std::find(State.selectedDoors.begin(), State.selectedDoors.end(), systemType) == State.selectedDoors.end();

					ImVec4 selectableColor = isPinned ? ImVec4(1.f, 0.f, 0.f, 1.f) :
	                    isSoftPinned ? ImVec4(1.f, 0.65f, 0.f, 1.f) :
						(State.RgbMenuTheme ? State.RgbColor : State.MenuThemeColor);

					if (isPinned || isSoftPinned || !isOpen) ImGui::PushStyleColor(ImGuiCol_Text, selectableColor);

					ImGui::PushID(static_cast<int>(systemType));
					if (ImGui::Selectable(GetDoorLabel(systemType), !isSelected)) {
						bool isCtrl = ImGui::IsKeyDown(0x11) || ImGui::IsKeyDown(0xA2) || ImGui::IsKeyDown(0xA3);
						bool isShifted = ImGui::IsKeyDown(0x10);

						if (isCtrl) {
							if (isShifted) {
								State.selectedDoors.clear();
							}
							else {
								auto it_sel = std::find(State.selectedDoors.begin(), State.selectedDoors.end(), systemType);
								if (it_sel != State.selectedDoors.end()) {
									State.selectedDoors.erase(it_sel);
								}
								else {
									State.selectedDoors.push_back(systemType);
								}
							}
						}
						else {
							State.selectedDoors = { systemType };
						}
					}

					ImGui::PopID();
					if (isPinned || isSoftPinned || !isOpen) ImGui::PopStyleColor(1);
				}
				ImGui::ListBoxFooter();
			}
			ImGui::EndChild();

			ImGui::SameLine();
			ImGui::BeginChild("doors#options", ImVec2(300, 0) * State.dpiScale, false, ImGuiWindowFlags_NoBackground);

			if (IsHost() && State.DisableSabotages) {
				ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Sabotages have been disabled.");
				ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Nothing can be sabotaged.");
			}

			if (AnimatedButton("Close All Doors"))
			{
				for (auto door : State.mapDoors)
				{
					State.rpcQueue.push(new RpcCloseDoorsOfType(door, false));
				}
			}

			if (AnimatedButton("Close Room Door"))
			{
				State.rpcQueue.push(new RpcCloseDoorsOfType(GetSystemTypes(GetTrueAdjustedPosition(*Game::pLocalPlayer)), false));
			}

			if (State.mapType == Settings::MapType::Pb || State.mapType == Settings::MapType::Airship || State.mapType == Settings::MapType::Fungle) {
				if (AnimatedButton("Open All Doors"))
				{
					for (auto door : State.mapDoors)
					{
						State.rpcQueue.push(new RpcOpenDoorsOfType(door));
					}
				}

				if (AnimatedButton("Open Room Door"))
				{
					State.rpcQueue.push(new RpcOpenDoorsOfType(GetSystemTypes(GetTrueAdjustedPosition(*Game::pLocalPlayer))));
				}
			}

            if (AnimatedButton("Pin All Doors")) {
                for (auto door : State.mapDoors) Pin(door, false);
            }
            const bool supportsSoftPin = State.mapType == Settings::MapType::Airship || State.mapType == Settings::MapType::Pb;
            if (supportsSoftPin && AnimatedButton("Soft Pin All Doors")) {
                for (auto door : State.mapDoors) Pin(door, true);
            }
            if (supportsSoftPin) ImGui::TextWrapped("Soft pins close doors 1.5 seconds after opening. Orange: soft pin. Red: hard pin.");
            if (AnimatedButton("Unpin All Doors")) {
                State.pinnedDoors.clear();
                State.softPinnedDoors.clear();
                State.doorOpenTimes.clear();
                State.pinnedDoorLastCheck.clear();
            }

			ImGui::NewLine();
			if (!State.selectedDoors.empty()) {
				if (AnimatedButton(State.selectedDoors.size() == 1 ? "Close Door" : "Close Doors")) {
					for (auto door : State.selectedDoors)
						State.rpcQueue.push(new RpcCloseDoorsOfType(door, false));
				}

                if (AnimatedButton(State.selectedDoors.size() == 1 ? "Pin Door" : "Pin Doors")) {
                    for (auto door : State.selectedDoors) Pin(door, false);
                }
                if (supportsSoftPin && AnimatedButton(State.selectedDoors.size() == 1 ? "Soft Pin Door" : "Soft Pin Doors")) {
                    for (auto door : State.selectedDoors) Pin(door, true);
                }
                if (AnimatedButton(State.selectedDoors.size() == 1 ? "Unpin Door" : "Unpin Doors")) {
                    for (auto door : State.selectedDoors) Unpin(door);
                }

				if ((State.mapType == Settings::MapType::Pb || State.mapType == Settings::MapType::Airship || State.mapType == Settings::MapType::Fungle) &&
					AnimatedButton(State.selectedDoors.size() == 1 ? "Open Door" : "Open Doors"))
				{
					for (auto door : State.selectedDoors)
						State.rpcQueue.push(new RpcOpenDoorsOfType(door));
				}
			}
			if (State.mapType == Settings::MapType::Pb || State.mapType == Settings::MapType::Airship || State.mapType == Settings::MapType::Fungle)
			{
				ImGui::Dummy(ImVec2(4, 4) * State.dpiScale);
				if (ToggleButton("Auto Open Doors on Use", &State.AutoOpenDoors)) State.Save();

				/*if (ToggleButton("Spam Open/Close Doors", &State.SpamDoors)) State.Save();*/
			}
			ImGui::EndChild();
		}
	}
}
