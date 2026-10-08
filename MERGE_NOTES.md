# Official + fanmade merge

Base: supplied official v5.1.1 snapshot, with additions from supplied fanmade v5.0.1. Neither source folder contained Git history; these labels come from Settings::Load, not verified upstream commits.

## Additions

- Soft pin on Polus/Airship: closes doors 1.5 seconds after opening. Works with official multiple-room selection and all doors.
- Hard pin fixes: suppress open updates, prevent auto-open minigames bypassing pins, and re-close pinned doors. Non-host pulse requests wait for the queue to drain.
- Pin modes are mutually exclusive per room. Unpin removes both. Decontamination is included in the door list, open/close commands, and pin controls; panic mode suspends enforcement.
- Polus Upper and Lower decontamination entries use each DeconSystem's actual door references. The installed map has four PlainDoors (IDs 12-15) in AllDoors, all sharing Room=Decontamination; grouping by that Room merges both chambers. Open and Close now affect BOTH physical doors of only the selected chamber. The host cancels the cycle and synchronizes its idle state before the physical door states in one reliable update, using native serializers. Status and soft pins read the actual doors, including when the cycle is idle. Hard-pin guards and auto-open handling resolve chamber membership by reference, so Lower pins cannot lock Upper. These controls remain host-only.
- Airship has a host-only Security door group: Kitchen hallway door 9 plus the two Electrical LeftExits. Close and hard pin block those three doors without closing the other Kitchen doors. Soft pin gives each opened door its own 1.5-second window. Unpin leaves the current door states unchanged.
- Select Security and use Open Door (or Open Room Door while in Security) to open all three; hard pins must first be removed. Open All and Kitchen opens leave the Electrical exits unchanged. Kitchen opens and card-swipe auto-open cannot bypass a Security hard pin.
- Security uses the game's LeftExits references and marks the normal and Electrical systems dirty for host replication. No added map objects or camera-system RPCs are used. If the expected door objects or metadata are unavailable, the controls report that instead of modifying another door.
- Radar > Show Others in Map: player icons, optional body icons, zoom scaling, meeting-position snapshots, and hiding over chat/admin. Scene changes clear cached positions.
- Settings > General > Show Keybinds: displays assigned shortcuts. The fanmade setting previously had no renderer.
- Autokill, saved setting, and shortcut editor, using official target selection. Limited to active gameplay, one attempt per 250 ms.
- Search clears on opening a result; radar size changes save immediately.
- Hook cleanup now also detaches Scientist, Tracker, and Detective updates, which were missing from the supplied official cleanup list.

Official game bindings, Influencer support, toasts, anti-exploit options, ESP settings, player/host tools, and multi-door selection are retained. Old generated headers, duplicate backups, old configuration paths, and outdated fanmade replacements were not copied.

## Build x64

Install Visual Studio Desktop development with C++, MSVC v143, and a Windows 10/11 SDK, then run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
```

Output: artifacts/x64/SickoMenu-merged-x64.zip, containing SickoMenu.dll, version.dll, license, notes, and SHA-256 hashes. Use either injection or the version proxy as described in README. GitHub Actions builds the same package on main pushes, pull requests, and manual dispatch; published releases receive the package on their own tag.

The build first compiles and runs 20 Security pin timer scenarios and Polus regression scenarios covering idle-cycle open doors, either physical door opening, separate chamber timers, hard-pin cancellation of pending cycles, unpin, and reset.

## Verification status

Seven source integration checks pass. Both x64 release configurations compiled and linked successfully in [GitHub Actions run 37203322887](https://github.com/peamixpelletin-cpu/SickoMenu/actions/runs/37203322887), producing SickoMenu.dll and version.dll in a ZIP with SHA-256 hashes. The local Visual Studio installation still lacks C++ targets; compilation was completed on GitHub's Windows runner. **In-game testing remains outstanding.**

Verify in a private test lobby before relying on gameplay behavior:

1. Soft-pin one/all rooms on Polus/Airship. Verify the 1.5-second delay, mode switching, and multiple-room unpin.
   On Polus, check that Decontamination (Upper) and Decontamination (Lower) can each be selected and controlled independently, including Ctrl multi-selection and all-door actions.
2. Check hard pins with manual and Auto Open Doors as host/client; verify panic mode.
   On Airship as host, select Security: close/open it and verify Kitchen door 9 and the two western Electrical exits from a second, unmodified client. Other Kitchen doors and the rest of the maze must retain their states. Check hard/soft pin, Kitchen/Open All bypass protection, Unpin All, meetings, panic mode, scene changes, and host migration. Open All must never open the Electrical exits. This new group has not yet been verified in-game.
3. Check map overlay alignment on every map, different zoom/window sizes, and flipped Skeld. The inherited fanmade map-fit calculation needs visual validation against the current game.
4. Check hiding over chat/admin, scene resets, and meeting snapshots resuming after exile.
5. Save/reload settings; test autokill cooldown, meeting/dead-player behavior, and panic mode.
6. Verify official toasts, Influencer, ESP, multi-selection, and anti-exploit features.

Existing fanmade profiles may be copied into the official SickoMenu/sicko-config folder after backing them up. Older chat-preset formats may need recreation in the official UI.
