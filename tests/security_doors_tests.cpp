#include "../user/security_doors_policy.h"
#include "../user/polus_decon_policy.h"
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
    Expect(PolusDecon::EntryState(true), 1, "upper entry uses native Enter state");
    Expect(PolusDecon::EntryState(false), 9, "lower entry uses native Enter HeadingUp state");
    Expect(PolusDecon::IsOpenState(0), false, "idle chamber is closed");
    Expect(PolusDecon::IsOpenState(10), false, "decontamination phase is closed");
    Expect(PolusDecon::IsOpenState(9), true, "entry phase has an open side");
    Expect(PolusDecon::IsOpenState(12), true, "exit phase has an open side");
    PolusDecon::PinTimer upper, lower;
    Expect(upper.Update(true, false, 0, 0), false, "hard pin does not retransmit idle state");
    Expect(upper.Update(true, false, 2, 0), true, "hard pin cancels a pending exit");
    Expect(upper.Update(true, false, 9, 0), true, "hard pin closes entry");
    Expect(upper.Update(false, true, 1, 0), false, "upper starts soft window");
    Expect(lower.Update(false, true, 9, 1000), false, "lower starts independent window");
    Expect(upper.Update(false, true, 1, 1499), false, "upper remains open at 1499ms");
    Expect(upper.Update(false, true, 1, 1500), true, "upper closes at 1500ms");
    Expect(lower.Update(false, true, 9, 1500), false, "upper expiry does not close lower");
    upper.Reset();
    Expect(lower.Update(false, true, 9, 2500), true, "upper unpin does not reset lower");
    Expect(lower.Update(false, true, 10, 2600), false, "closed phase resets timing");
    Expect(lower.Update(false, true, 12, 3000), false, "exit gets a new window");
    Expect(lower.Update(false, true, 12, 4500), true, "exit closes after its own window");
    Expect(lower.Update(false, false, 12, 5000), false, "unpin does not mutate chamber");
    Expect(lower.Update(false, true, 12, 6000), false, "repin starts fresh");
    Expect(lower.Update(false, true, 1, 7000), false, "new cycle resets soft timing");
    Expect(lower.Update(false, true, 1, 8500), true, "new cycle expires independently");
    std::cout << "Polus decontamination tests passed (22 scenarios).\n";
}
