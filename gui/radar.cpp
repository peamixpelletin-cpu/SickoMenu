#include "pch-il2cpp.h"
#include "radar.hpp"
#include "DirectX.h"
#include "utility.h"
#include "state.hpp"
#include "gui-helpers.hpp"
#include <algorithm>
#include <array>

namespace Radar {
	static std::array<Vector2, Game::MAX_PLAYERS> mapPlayerPositions = {};
	static std::array<bool, Game::MAX_PLAYERS> hasMapPlayerPosition = {};
	static bool mapPlayerPositionsFrozenForMeeting = false;

	ImU32 GetRadarPlayerColor(NetworkedPlayerInfo* playerData) {
		auto outfit = GetPlayerOutfit(playerData);
		if (outfit == NULL) return ImU32(0);

		return ImGui::ColorConvertFloat4ToU32(AmongUsColorToImVec4((GetPlayerColor(outfit->fields.ColorId))));
	}

	ImU32 GetRadarPlayerColorStatus(NetworkedPlayerInfo* playerData) {
		if (State.RevealRoles && playerData->fields.Role != nullptr)
			return ImGui::ColorConvertFloat4ToU32(AmongUsColorToImVec4(GetRoleColor(playerData->fields.Role)));
		else if (playerData->fields.IsDead)
			return ImGui::ColorConvertFloat4ToU32(AmongUsColorToImVec4(app::Palette__TypeInfo->static_fields->HalfWhite));
		else
			return ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0));
	}

	static bool CanRenderMapPlayer(PlayerControl* player, NetworkedPlayerInfo* playerData) {
		if (!player || !playerData || playerData->fields.Disconnected)
			return false;
		if (Game::pLocalPlayer && *Game::pLocalPlayer && player == *Game::pLocalPlayer)
			return false;
		if (!State.ShowRadar_Ghosts && playerData->fields.IsDead)
			return false;
		return playerData->fields.PlayerId < Game::MAX_PLAYERS;
	}

	static bool GetMapOverlayLayout(ImVec2& origin, float& mapScale) {
		if (maps.empty() || (size_t)State.mapType >= maps.size())
			return false;

		const auto& map = maps[(size_t)State.mapType];
		const float baseWidth = (float)map.mapImage.imageWidth * 0.5f;
		const float baseHeight = (float)map.mapImage.imageHeight * 0.5f;
		if (baseWidth <= 0.f || baseHeight <= 0.f)
			return false;

		const ImVec2 screenSize = DirectX::GetWindowSize();
		const float fitScale = (std::min)(screenSize.x / baseWidth, screenSize.y / baseHeight) * 0.92f;
		const ImVec2 mapSize(baseWidth * fitScale, baseHeight * fitScale);

		origin = ImVec2((screenSize.x - mapSize.x) * 0.5f, (screenSize.y - mapSize.y) * 0.5f);
		mapScale = fitScale;
		return mapScale > 0.f;
	}

	static ImVec2 WorldToMapScreenPosition(const Vector2& worldPosition, const ImVec2& origin, float mapScale) {
		const auto& map = maps[(size_t)State.mapType];
		const float radX = getMapXOffsetSkeld(map.x_offset) + (worldPosition.x * map.scale);
		const float radY = map.y_offset - (worldPosition.y * map.scale);
		return ImVec2(origin.x + radX * mapScale, origin.y + radY * mapScale);
	}

	static void DrawMapPlayerDot(ImDrawList* drawList, PlayerControl* player, NetworkedPlayerInfo* playerData, const Vector2& worldPosition, const ImVec2& origin, float mapScale) {
		const ImVec2 center = WorldToMapScreenPosition(worldPosition, origin, mapScale);
		const float radius = 4.5f * mapScale;
		const float outlineThickness = 2.0f * (std::max)(0.75f, mapScale);
		const ImU32 color = GetRadarPlayerColor(playerData);
		const ImU32 statusColor = GetRadarPlayerColorStatus(playerData);

		auto localData = (Game::pLocalPlayer && *Game::pLocalPlayer) ? GetPlayerData(*Game::pLocalPlayer) : nullptr;
		if (localData && (State.RevealRoles || PlayerIsImpostor(localData)) && PlayerIsImpostor(playerData)) {
			const ImVec2 topLeft(center.x - radius, center.y - radius);
			const ImVec2 bottomRight(center.x + radius, center.y + radius);
			drawList->AddRectFilled(topLeft, bottomRight, color, 1.f * mapScale);
			drawList->AddRect(topLeft, bottomRight, statusColor, 1.f * mapScale, 15, outlineThickness);
			return;
		}

		drawList->AddCircleFilled(center, radius, color);
		drawList->AddCircle(center, radius + (0.5f * mapScale), statusColor, 0, outlineThickness);
	}

	static void DrawMapPlayerIcon(ImDrawList* drawList, PlayerControl* player, NetworkedPlayerInfo* playerData, const Vector2& worldPosition, const ImVec2& origin, float mapScale) {
		const auto& map = maps[(size_t)State.mapType];
		const float xOffset = getMapXOffsetSkeld(map.x_offset);
		const float yOffset = map.y_offset;

		IconTexture icon = icons.at(ICON_TYPES::PLAYER);
		IconTexture visor = icons.at(ICON_TYPES::PLAYERVISOR);
		const float halfImageWidth = icon.iconImage.imageWidth * icon.scale * 0.5f;
		const float halfImageHeight = icon.iconImage.imageHeight * icon.scale * 0.5f;
		const float radX = xOffset + (worldPosition.x - halfImageWidth) * map.scale;
		const float radY = yOffset - (worldPosition.y - halfImageHeight) * map.scale;
		const float radXMax = xOffset + (worldPosition.x + halfImageWidth) * map.scale;
		const float radYMax = yOffset - (worldPosition.y + halfImageHeight) * map.scale;

		const ImVec2 p_min(origin.x + radX * mapScale, origin.y + radY * mapScale);
		const ImVec2 p_max(origin.x + radXMax * mapScale, origin.y + radYMax * mapScale);

		drawList->AddImage((void*)icon.iconImage.shaderResourceView,
			p_min, p_max,
			ImVec2(0.0f, 1.0f),
			ImVec2(1.0f, 0.0f),
			GetRadarPlayerColor(playerData));

		drawList->AddImage((void*)visor.iconImage.shaderResourceView,
			p_min, p_max,
			ImVec2(0.0f, 1.0f),
			ImVec2(1.0f, 0.0f),
			(State.RevealRoles && playerData->fields.Role) ?
			ImGui::GetColorU32(AmongUsColorToImVec4(GetRoleColor(playerData->fields.Role))) :
			ImGui::GetColorU32(AmongUsColorToImVec4(Palette__TypeInfo->static_fields->VisorColor)));

		if (playerData->fields.IsDead)
			drawList->AddImage((void*)icons.at(ICON_TYPES::CROSS).iconImage.shaderResourceView,
				p_min, p_max,
				ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f), IM_COL32_WHITE);
	}

	static void CaptureMapPlayerPositionsInternal() {
		if (!IsInGame())
			return;

		std::array<bool, Game::MAX_PLAYERS> seenPlayers = {};
		for (auto player : GetAllPlayerControl()) {
			if (!player)
				continue;

			auto playerData = GetPlayerData(player);
			if (!playerData || playerData->fields.Disconnected || playerData->fields.PlayerId >= Game::MAX_PLAYERS)
				continue;

			const auto playerId = playerData->fields.PlayerId;
			mapPlayerPositions[playerId] = app::PlayerControl_GetTruePosition(player, NULL);
			hasMapPlayerPosition[playerId] = true;
			seenPlayers[playerId] = true;
		}

		for (size_t i = 0; i < hasMapPlayerPosition.size(); i++) {
			if (!seenPlayers[i])
				hasMapPlayerPosition[i] = false;
		}
	}

	void CaptureMapPlayerPositions() {
		if (mapPlayerPositionsFrozenForMeeting)
			return;

		CaptureMapPlayerPositionsInternal();
	}

	void CaptureMeetingMapPlayerPositions() {
		if (mapPlayerPositionsFrozenForMeeting)
			return;

		CaptureMapPlayerPositionsInternal();
		mapPlayerPositionsFrozenForMeeting = true;
	}

	void ResetMapPlayerPositionFreeze() {
		mapPlayerPositionsFrozenForMeeting = false;
	}

	void SquareConstraint(ImGuiSizeCallbackData* data)
	{
		data->DesiredSize = ImVec2(data->DesiredSize.x, data->DesiredSize.y);
	}

	void OnClick() {
		if (!ImGui::IsKeyPressed(VK_SHIFT) && !ImGui::IsKeyDown(VK_SHIFT) && !ImGui::IsKeyDown(VK_CONTROL) && (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Right))) {
			ImVec2 mouse = ImGui::GetMousePos();
			ImVec2 winpos = ImGui::GetWindowPos();
			ImVec2 winsize = ImGui::GetWindowSize();

			if (mouse.x < winpos.x
				|| mouse.x > winpos.x + winsize.x
				|| mouse.y < winpos.y
				|| mouse.y > winpos.y + winsize.y)
				return;

			const auto& map = maps[(size_t)State.mapType];
			float xOffset = getMapXOffsetSkeld(map.x_offset) + (float)State.RadarExtraWidth;
			float yOffset = map.y_offset + (float)State.RadarExtraHeight;

			Vector2 target = {
				((mouse.x - winpos.x) / State.dpiScale - xOffset) / map.scale,
				(((mouse.y - winpos.y) / State.dpiScale - yOffset) * -1.F) / map.scale
			};

			static int tpDelay = 0;

			if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) State.rpcQueue.push(new RpcSnapTo(target));
			else if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
				if (tpDelay <= 0) {
					State.rpcQueue.push(new RpcSnapTo(target));
					tpDelay = int(0.1 * GetFps());
				}
				else tpDelay--;
			}
		}
		if (State.TeleportEveryone && !(ImGui::IsKeyDown(VK_CONTROL) || ImGui::IsKeyDown(VK_SHIFT)) && (ImGui::IsKeyPressed(0x12) || ImGui::IsKeyDown(0x12)) && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			ImVec2 mouse = ImGui::GetMousePos();
			ImVec2 winpos = ImGui::GetWindowPos();
			ImVec2 winsize = ImGui::GetWindowSize();

			if (mouse.x < winpos.x
				|| mouse.x > winpos.x + winsize.x
				|| mouse.y < winpos.y
				|| mouse.y > winpos.y + winsize.y)
				return;

			const auto& map = maps[(size_t)State.mapType];
			float xOffset = getMapXOffsetSkeld(map.x_offset) + (float)State.RadarExtraWidth;
			float yOffset = map.y_offset + (float)State.RadarExtraHeight;

			Vector2 target = {
				((mouse.x - winpos.x) / State.dpiScale - xOffset) / map.scale,
				(((mouse.y - winpos.y) / State.dpiScale - yOffset) * -1.F) / map.scale
			};

			for (auto player : GetAllPlayerControl()) {
				State.rpcQueue.push(new RpcForceSnapTo(player, target));
			}
		}
	}

	void Init() {
		ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(FLT_MAX, FLT_MAX), SquareConstraint);
		ImGui::SetNextWindowBgAlpha(0.F);
	}

	bool init = false;
	void Render() {
		if (!init)
			Radar::Init();

		const auto& map = maps[(size_t)State.mapType];
		ImGui::SetNextWindowSize(ImVec2((float)map.mapImage.imageWidth * 0.5F + 10.F + 2.f * State.RadarExtraWidth, (float)map.mapImage.imageHeight * 0.5f + 10.f + 2.f * State.RadarExtraHeight) * State.dpiScale, ImGuiCond_None);

		if (State.LockRadar)
			ImGui::Begin("Radar", &State.ShowRadar, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
		else
			ImGui::Begin("Radar", &State.ShowRadar, ImGuiWindowFlags_NoDecoration);

		ImVec2 winpos = ImGui::GetWindowPos();

		if (State.RadarBorder) {
			const ImVec2 points[] = { {winpos.x + 3.f * State.dpiScale, winpos.y + 1.f * State.dpiScale}, {winpos.x - 5.f * State.dpiScale + ImGui::GetWindowWidth(), winpos.y + 1.f * State.dpiScale}, {winpos.x - 5.f * State.dpiScale + ImGui::GetWindowWidth(), winpos.y + ImGui::GetWindowHeight() - 4.f * State.dpiScale}, {winpos.x + 3.f * State.dpiScale, winpos.y + ImGui::GetWindowHeight() - 4.f * State.dpiScale}, {winpos.x + 3.f * State.dpiScale, winpos.y + 1.f * State.dpiScale} };
			for (size_t i = 0; i < std::size(points); i++) {
				ImGui::GetCurrentWindow()->DrawList->AddLine(points[i], points[i + 1], State.RgbMenuTheme ? ImGui::GetColorU32(ImVec4(State.RgbColor.x, State.RgbColor.y, State.RgbColor.z, State.SelectedColor.w)) : ImGui::GetColorU32(State.SelectedColor), 2.f);
			}
		}

		ImVec4 RadarColor = ImVec4(1.f, 1.f, 1.f, 0.75f);
		if (State.RgbMenuTheme)
			RadarColor = { State.RgbColor.x, State.RgbColor.y, State.RgbColor.z, State.SelectedColor.w };
		else
			RadarColor = State.SelectedColor;

		GameOptions options;

		ImGui::Image((void*)map.mapImage.shaderResourceView,
			ImVec2((float)map.mapImage.imageWidth * 0.5F, (float)map.mapImage.imageHeight * 0.5F) * State.dpiScale,
			ImVec2((float)State.RadarExtraWidth * State.dpiScale, (float)State.RadarExtraHeight * State.dpiScale),
			(State.FlipSkeld && State.mapType == Settings::MapType::Ship) ? ImVec2(1.0f, 0.0f) : ImVec2(0.0f, 0.0f),
			(State.FlipSkeld && State.mapType == Settings::MapType::Ship) ? ImVec2(0.0f, 1.0f) : ImVec2(1.0f, 1.0f),
			RadarColor);

		for (auto player : GetAllPlayerControl()) {
			auto playerData = GetPlayerData(player);

			if (!playerData || (!State.ShowRadar_Ghosts && playerData->fields.IsDead))
				continue;

			if (State.RadarDrawIcons)
				drawPlayerIcon(player, winpos, GetRadarPlayerColor(playerData));
			else
				drawPlayerDot(player, winpos, GetRadarPlayerColor(playerData), GetRadarPlayerColorStatus(playerData));
		}

		if (State.ShowRadar_DeadBodies) {
			for (auto deadBody : GetAllDeadBodies()) {
				auto playerId = deadBody->fields.ParentId;
				auto playerData = GetPlayerDataById(playerId);

				if (std::find(State.validDeadBodyIds.begin(), State.validDeadBodyIds.end(), playerId) == State.validDeadBodyIds.end())
					continue; // the dead body has been fully dissolved or invalid in this case, don't render it

				if (State.RadarDrawIcons)
					drawDeadPlayerIcon(deadBody, winpos, GetRadarPlayerColor(playerData));
				else
					drawDeadPlayerDot(deadBody, winpos, GetRadarPlayerColor(playerData));
			}
		}

		if (State.ShowRadar_RightClickTP)
			OnClick();

		ImGui::End();
	}

	void RenderMapPlayers() {
		ImVec2 mapOrigin;
		float mapScale = 1.f;
		if (!GetMapOverlayLayout(mapOrigin, mapScale))
			return;

		const ImVec2 screenSize = DirectX::GetWindowSize();
		ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(screenSize, ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.f);

		ImGui::Begin("Map Player Positions", nullptr,
			ImGuiWindowFlags_NoDecoration
			| ImGuiWindowFlags_NoInputs
			| ImGuiWindowFlags_NoSavedSettings
			| ImGuiWindowFlags_NoFocusOnAppearing
			| ImGuiWindowFlags_NoBackground);

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		for (auto player : GetAllPlayerControl()) {
			if (!player)
				continue;

			auto playerData = GetPlayerData(player);
			if (!CanRenderMapPlayer(player, playerData))
				continue;

			const auto playerId = playerData->fields.PlayerId;
			if (!hasMapPlayerPosition[playerId])
				continue;

			const Vector2& playerPosition = mapPlayerPositions[playerId];
			if (State.RadarDrawIcons)
				DrawMapPlayerIcon(drawList, player, playerData, playerPosition, mapOrigin, mapScale);
			else
				DrawMapPlayerDot(drawList, player, playerData, playerPosition, mapOrigin, mapScale);
		}

		ImGui::End();
	}
}
