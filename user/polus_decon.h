#pragma once
#include "pch-il2cpp.h"

namespace PolusDecon {
    bool IsGroup(app::SystemTypes__Enum room);
    void RefreshEntries();
    const char* Label(app::SystemTypes__Enum room);
    bool ReadState(app::SystemTypes__Enum room, bool& anyOpen);
    bool CanControl(app::SystemTypes__Enum room, bool notify = false);
    bool SetOpen(app::SystemTypes__Enum room, bool open);
    bool IsHardPinned(app::SystemTypes__Enum room);
    void UpdatePins();
    void Reset(app::SystemTypes__Enum room);
    void Reset();
}
