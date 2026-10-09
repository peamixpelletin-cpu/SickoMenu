#include "pch-il2cpp.h"
#include "doors_tab.h"
#include "game.h"
#include "gui-helpers.hpp"
#include "imgui/imgui.h"
#include "state.hpp"
#include "security_doors.h"
#include "polus_decon.h"
#include "electrical_maze.h"
#include <cmath>
#include "utility.h"
#include "gui-helpers.hpp"

using namespace std::string_view_literals;

namespace DoorsTab {
    static const char* GetDoorLabel(SystemTypes__Enum room) {
        if (KitchenEast::IsGroup(room)) return "KitchenEast";
        if (PolusDecon::IsGroup(room)) return PolusDecon::Label(room);
        return TranslateSystemTypes(room);
    }

    static void RenderElectrical() {
        if (!ImGui::CollapsingHeader("Electrical door map", ImGuiTreeNodeFlags_DefaultOpen)) return;
        auto maze = ElectricalMaze::Read();
        if (!maze.ready) {
            ImGui::TextWrapped("Electrical doors unavailable. Enter an Airship game to see the map.");
            return;
        }
        if (!maze.host) {
            ImGui::TextUnformatted("View only (host controls doors).");
        }
        else ImGui::TextWrapped("Click a door to open or close it for the lobby.");
        ImGui::TextUnformatted("Green: open   Red: closed");
        const float width = ImGui::GetContentRegionAvail().x;
        const float height = width * 0.82f;
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        auto draw = ImGui::GetWindowDrawList();
        float minX = maze.doors[0].x, maxX = minX, minY = maze.doors[0].y, maxY = minY;
        for (const auto& door : maze.doors) {
            if (!std::isfinite(door.x) || !std::isfinite(door.y)) return;
            minX = (std::min)(minX, door.x); maxX = (std::max)(maxX, door.x);
            minY = (std::min)(minY, door.y); maxY = (std::max)(maxY, door.y);
        }
        minX -= 1.0f; maxX += 2.8f; minY -= 2.0f; maxY += 2.3f;
        const auto point = [&](float x, float y) {
            return ImVec2(origin.x + (x - minX) / (maxX - minX) * width,
                origin.y + (maxY - y) / (maxY - minY) * height);
        };
        draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), IM_COL32(23, 28, 35, 255), 5.f);
        // Schematic walls use the verified stock door ordering; dots themselves
        // use live world positions, so the control always follows the real door.
        const float left = maze.doors[10].x, midLeft = maze.doors[5].x, midRight = maze.doors[6].x;
        const float right = 2.f * maze.doors[0].x - midRight;
        const float top = maze.doors[7].y + 1.7f, upper = maze.doors[2].y;
        const float lower = maze.doors[0].y, bottom = maze.doors[9].y - 1.7f;
        const auto wall = [&](float x1, float y1, float x2, float y2) {
            draw->AddLine(point(x1, y1), point(x2, y2), IM_COL32(125, 137, 154, 255), 2.f * State.dpiScale);
        };
        wall(left, top, right, top); wall(left, top, left, lower);
        wall(right, top, right, bottom); wall(midLeft, bottom, right, bottom);
        wall(left, lower, right, lower); wall(left, upper, right, upper);
        wall(midLeft, top, midLeft, bottom); wall(midRight, top, midRight, bottom);
        const char* names[] = { "Bottom right / north", "Center / south", "Top right / south",
            "Top center / south", "Top left / south", "Center left / east", "Center right / west",
            "Top right / west", "Top left / east", "Bottom right / west", "Security exit / upper", "Security exit / lower" };
        const float radius = 8.f * State.dpiScale;
        for (unsigned i = 0; i < ElectricalMaze::DoorCount; ++i) {
            const auto& door = maze.doors[i];
            const auto p = point(door.x, door.y);
            ImGui::SetCursorScreenPos(ImVec2(p.x - radius, p.y - radius));
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::InvisibleButton("electrical-door", ImVec2(radius * 2, radius * 2)) && maze.host)
                ElectricalMaze::Queue(i, !door.open, maze.generation);
            const bool hovered = ImGui::IsItemHovered();
            draw->AddCircleFilled(p, radius, door.open ? IM_COL32(42, 193, 110, 255) : IM_COL32(233, 75, 83, 255));
            draw->AddCircle(p, radius, hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(15, 18, 24, 255), 0, 2.f);
            if (hovered) {
                ImGui::BeginTooltip();
                ImGui::Text("Door %u - %s", i + 1, names[i]);
                ImGui::TextUnformatted(door.open ? "Open" : "Closed");
                if (!maze.host) ImGui::TextUnformatted("The host controls this door.");
                ImGui::EndTooltip();
            }
            ImGui::PopID();
        }
        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + height));
        ImGui::Dummy(ImVec2(width, 5.f * State.dpiScale));
        if (!maze.status.empty()) ImGui::TextWrapped("%s", maze.status.c_str());
        ImGui::Separator();
    }

    static void Unpin(SystemTypes__Enum room) {
        if (KitchenEast::IsGroup(room)) KitchenEast::Reset();
        if (PolusDecon::IsGroup(room)) PolusDecon::Reset(room);
        State.pinnedDoors.erase(std::remove(State.pinnedDoors.begin(), State.pinnedDoors.end(), room), State.pinnedDoors.end());
        State.softPinnedDoors.erase(std::remove(State.softPinnedDoors.begin(), State.softPinnedDoors.end(), room), State.softPinnedDoors.end());
        State.doorOpenTimes.clear();
        State.pinnedDoorLastCheck.erase(room);
    }

    static void Pin(SystemTypes__Enum room, bool soft) {
        if (KitchenEast::IsGroup(room) && !KitchenEast::CanControl(true)) return;
        if (PolusDecon::IsGroup(room) && !PolusDecon::CanControl(room, true)) return;
        Unpin(room);
        (soft ? State.softPinnedDoors : State.pinnedDoors).push_back(room);
        State.rpcQueue.push(new RpcCloseDoorsOfType(room, false));
    }

	void Render() {
        if (IsInGame()) PolusDecon::RefreshEntries();
		if (IsInGame() && !State.mapDoors.empty()) {
			ImGui::SameLine(100 * State.dpiScale);
			ImGui::BeginChild("doors#list", ImVec2(200, 0) * State.dpiScale, true, ImGuiWindowFlags_NoBackground);
			bool shouldEndListBox = ImGui::ListBoxHeader("###doors#list", ImVec2(200, 150) * State.dpiScale);
			if (shouldEndListBox) {
				for (auto systemType : State.mapDoors) {
					bool isOpen = false;
					if (KitchenEast::IsGroup(systemType)) {
                        KitchenEast::ReadState(isOpen);
                    }
                    else if (PolusDecon::IsGroup(systemType)) {
                        PolusDecon::ReadState(systemType, isOpen);
                    }
                    else {
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

			if (State.mapType == Settings::MapType::Airship) RenderElectrical();

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
            if (supportsSoftPin) ImGui::TextWrapped("Soft pin: 1.5 s. Orange = soft; red = hard.");
            if (AnimatedButton("Unpin All Doors")) {
                KitchenEast::Reset();
                PolusDecon::Reset();
                State.pinnedDoors.clear();
                State.softPinnedDoors.clear();
                State.doorOpenTimes.clear();
                State.pinnedDoorLastCheck.clear();
            }

			ImGui::NewLine();
            if (State.mapType == Settings::MapType::Pb) {
                ImGui::TextUnformatted("Decontamination: both doors per chamber.");
            }
            if (State.mapType == Settings::MapType::Airship) {
                ImGui::TextUnformatted("KitchenEast: east Kitchen door (host).");
                bool anyOpen = false;
                if (!KitchenEast::ReadState(anyOpen))
                    ImGui::TextWrapped("KitchenEast is unavailable for this map layout.");
            }
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
