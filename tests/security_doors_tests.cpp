#include "../user/security_doors_policy.h"
#include "../user/polus_decon_policy.h"
#include "../user/electrical_maze_policy.h"
#include <cstdlib>
#include <iostream>

using namespace KitchenEast;

static void Expect(unsigned actual, unsigned expected, const char* scenario) {
    if (actual != expected) {
        std::cerr << scenario << ": expected " << expected << ", got " << actual << '\n';
        std::exit(1);
    }
}

int main() {
    PinTimer timer;
    Expect(timer.Update(PinMode::Hard, true, 0), true, "hard pin closes KitchenEast");
    Expect(timer.Update(PinMode::Hard, false, 1), false, "closed door does not retransmit");
    Expect(timer.Update(PinMode::Soft, true, 0), false, "soft pin starts window");
    Expect(timer.Update(PinMode::Soft, true, 1499), false, "soft pin permits 1499ms");
    Expect(timer.Update(PinMode::Soft, true, 1500), true, "soft pin closes at 1500ms");
    Expect(timer.Update(PinMode::Soft, false, 1501), false, "close clears window");
    Expect(timer.Update(PinMode::Soft, true, 1600), false, "new opening gets fresh window");
    Expect(timer.Update(PinMode::Soft, true, 3100), true, "new window expires");
    Expect(timer.Update(PinMode::None, true, 4000), false, "unpin does not change door");
    Expect(timer.Update(PinMode::Soft, true, 5000), false, "repin clears old time");
    timer.Reset();
    Expect(timer.Update(PinMode::Soft, true, 8000), false, "scene/panic reset clears time");
    Expect(timer.Update(PinMode::Hard, true, 8001), true, "hard pin overrides soft window");
    std::cout << "KitchenEast pin tests passed (12 scenarios).\n";

    // Live door references are independent from room-based normal doors.
    std::array<bool, 12> maze{}; maze.fill(true);
    std::array<bool*, 12> refs{};
    for (unsigned i = 0; i < refs.size(); ++i) refs[i] = &maze[i];
    unsigned calls = 0;
    const auto setMaze = [&](bool* door, bool open) { ++calls; *door = open; return true; };
    const auto apply = [&](ElectricalMaze::Command command, bool host, bool experimental) {
        return ElectricalMaze::ApplySelected(command, 7, host, experimental, refs, setMaze);
    };
    Expect(apply({10, false, 7}, true, false), true, "host can close upper west exit");
    Expect(maze[10], false, "selected exit closes");
    bool othersOpen = true;
    for (unsigned i = 0; i < maze.size(); ++i) if (i != 10) othersOpen &= maze[i];
    Expect(othersOpen, true, "other eleven maze doors stay unchanged");
    Expect(calls, 1, "one command calls exactly one setter");
    Expect(apply({11, false, 7}, false, false), false, "non-host disabled by default");
    Expect(maze[11], true, "blocked command cannot change lower exit");
    Expect(apply({11, false, 7}, false, true), true, "explicit experimental command allowed");
    Expect(maze[11], false, "experimental command changes selected local door");
    Expect(apply({11, true, 6}, true, true), false, "old scene generation rejected");
    Expect(apply({12, false, 7}, true, true), false, "out-of-range index rejected");
    Expect(apply({0, false, 7}, false, false), false, "losing host or disabling experiment blocks queued request");
    Expect(calls, 2, "rejected commands never call setter");
    Expect(apply({10, true, 7}, true, false), true, "host can reopen a single door");
    Expect(maze[10] && !maze[11], true, "reopening upper does not reopen lower");
    std::cout << "Electrical maze command tests passed (14 scenarios).\n";
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
