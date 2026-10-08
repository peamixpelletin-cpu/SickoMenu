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
    // The installed map has four PlainDoors with the same Room=18.
    // Selection must use the two explicit references, never all matching Rooms.
    struct Door { int room = 18; bool open = false; } d12, d13, d14, d15;
    const std::array lowerPair{ &d12, &d13 }, upperPair{ &d14, &d15 };
    const auto set = [](Door* door, bool open) { door->open = open; };
    PolusDecon::SetPair(lowerPair, true, set);
    Expect(d12.open && d13.open, true, "Lower open opens both physical doors");
    Expect(d14.open || d15.open, false, "Lower open leaves Upper closed despite shared Room");
    PolusDecon::SetPair(upperPair, true, set);
    Expect(d12.open && d13.open && d14.open && d15.open, true, "opening both chambers opens all four");
    PolusDecon::SetPair(lowerPair, false, set);
    Expect(d12.open || d13.open, false, "Lower close closes both physical doors");
    Expect(d14.open && d15.open, true, "Lower close leaves Upper open");
    PolusDecon::SetPair(upperPair, false, set);
    Expect(d14.open || d15.open, false, "Upper close closes both physical doors");
    Expect(PolusDecon::AnyOpen({true, false}), true, "status detects first physical door");
    Expect(PolusDecon::AnyOpen({false, true}), true, "status detects second physical door");
    Expect(PolusDecon::AnyOpen({false, false}), false, "status requires an open physical door");

    PolusDecon::PinTimer upper, lower;
    Expect(upper.Update(true, false, 0, {false, false}, 0), false, "hard pin does not retransmit closed idle pair");
    Expect(upper.Update(true, false, 2, {false, false}, 0), true, "hard pin cancels a pending exit");
    Expect(upper.Update(true, false, 0, {true, true}, 0), true, "hard pin sees open doors even with idle cycle");
    Expect(upper.Update(false, true, 0, {true, true}, 0), false, "manual pair-open starts soft window");
    Expect(lower.Update(false, true, 0, {false, true}, 1000), false, "Lower starts independent window");
    Expect(upper.Update(false, true, 0, {true, true}, 1499), false, "Upper remains open at 1499ms");
    Expect(upper.Update(false, true, 0, {true, true}, 1500), true, "Upper closes at 1500ms despite idle cycle");
    Expect(lower.Update(false, true, 0, {true, true}, 1500), false, "Upper expiry does not close Lower");
    upper.Reset();
    Expect(lower.Update(false, true, 0, {true, true}, 2500), true, "Upper unpin does not restart Lower window");
    Expect(lower.Update(false, true, 2, {false, false}, 2600), false, "closed pair resets soft timer");
    Expect(lower.Update(false, true, 12, {true, false}, 3000), false, "native exit gets a fresh window");
    Expect(lower.Update(false, true, 12, {true, false}, 4500), true, "native exit closes after its window");
    Expect(lower.Update(false, false, 0, {true, true}, 5000), false, "unpin never changes either door");
    Expect(lower.Update(false, true, 0, {true, true}, 6000), false, "repin starts fresh");
    lower.Reset();
    Expect(lower.Update(false, true, 0, {true, true}, 8000), false, "scene reset clears old opening time");
    Expect(lower.Update(true, false, 0, {false, true}, 8001), true, "hard pin closes either individual door");
    std::cout << "Polus decontamination tests passed (25 scenarios).\n";
}
