#pragma once
#include "pch-il2cpp.h"

namespace KitchenEast {
    bool IsGroup(app::SystemTypes__Enum room);
    bool IsKitchenDoor(app::OpenableDoor* door);
    bool IsHardPinnedKitchen(app::OpenableDoor* door);
    bool ReadState(bool& anyOpen);
    bool CanControl(bool notify = false);
    bool SetOpen(bool open);
    void CloseKitchen();
    void UpdatePins();
    void Reset();
}
