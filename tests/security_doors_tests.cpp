#include "../user/security_doors_policy.h"
#include <cstdlib>
#include <iostream>

using namespace SecurityDoors;

static void Expect(unsigned actual, unsigned expected, const char* scenario) {
    if (actual != expected) {
        std::cerr << scenario << ": expected " << expected << ", got " << actual << '\n';
        std::exit(1);
    }
}

int main() {
    PinTimer timer;
    Expect(timer.Update(PinMode::Hard, { true, true, true }, 0), AllMask, "hard pin closes all three");
    Expect(timer.Update(PinMode::Hard, { true, false, false }, 1), KitchenMask, "only reopened Kitchen door changes");
    Expect(timer.Update(PinMode::Hard, { false, true, true }, 2), ElectricalMask, "Electrical exits close independently");
    Expect(timer.Update(PinMode::Hard, { false, false, false }, 3), 0, "closed doors do not repeatedly transmit");

    timer.Reset();
    Expect(timer.Update(PinMode::Soft, { true, true, true }, 0), 0, "soft pin starts open window");
    Expect(timer.Update(PinMode::Soft, { true, true, true }, 1499), 0, "soft pin permits 1499ms");
    Expect(timer.Update(PinMode::Soft, { true, true, true }, 1500), AllMask, "soft pin closes at 1500ms");
    Expect(timer.Update(PinMode::Soft, { false, false, false }, 1501), 0, "successful close resets timers");
    Expect(timer.Update(PinMode::Soft, { true, false, false }, 1600), 0, "a new opening gets a new window");
    Expect(timer.Update(PinMode::Soft, { true, false, false }, 3100), KitchenMask, "new opening expires");

    timer.Reset();
    Expect(timer.Update(PinMode::Soft, { true, false, false }, 0), 0, "Kitchen opens first");
    Expect(timer.Update(PinMode::Soft, { true, true, false }, 1000), 0, "upper exit opens later");
    Expect(timer.Update(PinMode::Soft, { true, true, false }, 1500), KitchenMask, "upper exit retains its own window");
    Expect(timer.Update(PinMode::Soft, { false, true, true }, 2000), 0, "lower exit opens last");
    Expect(timer.Update(PinMode::Soft, { false, true, true }, 2500), 2, "only upper exit expires");
    Expect(timer.Update(PinMode::Soft, { false, false, true }, 3500), 4, "lower exit expires separately");

    Expect(timer.Update(PinMode::None, { true, true, true }, 4000), 0, "unpin never opens or closes doors");
    Expect(timer.Update(PinMode::Soft, { true, true, true }, 5000), 0, "repin clears old timing");
    timer.Reset();
    Expect(timer.Update(PinMode::Soft, { true, true, true }, 6000), 0, "scene/panic reset clears timers");
    Expect(timer.Update(PinMode::Hard, { true, true, true }, 6100), AllMask, "hard pin overrides a soft window");
    std::cout << "Security door pin tests passed (20 scenarios).\n";
}
