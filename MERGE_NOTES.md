# Official + fanmade merge

Base: supplied official v5.1.1 snapshot, with additions from supplied fanmade v5.0.1. Neither source folder contained Git history; these labels come from Settings::Load, not verified upstream commits.

## Additions

- Soft pin on Polus/Airship: closes doors 1.5 seconds after opening. Works with official multiple-room selection and all doors.
- Hard pin fixes: suppress open updates, prevent auto-open minigames bypassing pins, and re-close pinned doors. Non-host pulse requests wait for the queue to drain.
- Pin modes are mutually exclusive per room. Unpin removes both. Decontamination is included in the door list, open/close commands, and pin controls; panic mode suspends enforcement.
- Polus Upper and Lower decontamination entries use each DeconSystem's actual door references. The installed map has four PlainDoors (IDs 12-15) in AllDoors, all sharing Room=Decontamination; grouping by that Room merges both chambers. Open and Close now affect BOTH physical doors of only the selected chamber. The host cancels the cycle and synchronizes its idle state before the physical door states in one reliable update, using native serializers. Status and soft pins read the actual doors, including when the cycle is idle. Hard-pin guards and auto-open handling resolve chamber membership by reference, so Lower pins cannot lock Upper. Hosts keep this ordered synchronization. Non-hosts now use the original utility's ordinary door RPCs: Open requests both selected IDs; Close requests their shared physical room and then restores previously open doors outside the selected pair. Client hard pins use the same 50 ms pulse as ordinary doors, including when locally closed, wait for the RPC queue to drain, and suppress local reopen attempts immediately. Soft pins retain the 1.5-second window. Clients cannot cancel the host's cycle, so an active cycle can override requests, and a shared-room close may briefly affect the other chamber. Visibility to other clients is not yet verified.
- Airship KitchenEast replaces the former Security composite. It controls only Kitchen hallway door 9, with independent hard/soft pins. Electrical exits are no longer changed by KitchenEast or ordinary Open All/Close All actions. Kitchen and card-swipe auto-open respect a KitchenEast hard pin.
- Airship Electrical has a clickable schematic with twelve red/green door dots and live world positions. Each click changes one StaticDoor, preserving the other eleven. As host, native ShipStatus replication sends the change. UI reads a synchronized snapshot; game-object access and network sends run on the game thread. Scene changes, meetings, panic mode and stale command generations clear pending actions.
- The experimental non-host Electrical state sender was removed after the user reported being kicked from an official server. Electrical remains a live map for non-hosts and a control map for hosts. No replacement official-server bypass has been verified.
- Research checks: Waterway's public Room.handleDataMessage path lacks a visible host check before processing and forwarding Data, but deployment/plugin behavior is unverified. Impostor validates host authority. Hydra v1.9.0 documents that its older broad bypass was patched. See [Electrical research notes](https://github.com/peamixpelletin-cpu/SickoMenu/blob/main/docs/ELECTRICAL_RESEARCH.md).
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

The build first compiles and runs 12 KitchenEast pin scenarios, 14 Electrical command scenarios, 16 non-host decontamination request scenarios, 5 hard-pin cadence scenarios and 25 Polus regression scenarios covering idle-cycle open doors, either physical door opening, separate chamber timers, hard-pin cancellation of pending cycles, unpin, and reset.

## Verification status

Seven source integration checks pass. Each GitHub Actions build compiles and runs the native regression tests before building both x64 DLL configurations; use the workflow run associated with the downloaded artifact for its result. The local Visual Studio installation lacks C++ targets, so compilation runs on GitHub's Windows runner. **In-game testing remains outstanding, including remote visibility and cycle interactions of non-host Polus requests.**

Verify in a private test lobby before relying on gameplay behavior:

1. Soft-pin one/all rooms on Polus/Airship. Verify the 1.5-second delay, mode switching, and multiple-room unpin.
   On Polus, check that Decontamination (Upper) and Decontamination (Lower) can each be selected and controlled independently, including Ctrl multi-selection and all-door actions.
2. Check hard pins with manual and Auto Open Doors as host/client; verify panic mode.
   On Airship as host, select KitchenEast and verify only Kitchen door 9 changes from a second, unmodified client. Test its hard/soft pin and Kitchen/Open All bypass protection. In Electrical's map toggle each of the twelve dots, especially the two west exits, and verify other doors retain their states. Repeat across meetings, panic, scene changes and host migration. Non-host Electrical clicks must leave every door unchanged and emit no state packet. On Polus as non-host, verify each chamber's two doors from an unmodified observer, including opening/closing while the other chamber is open and while a native cycle is running. Record temporary changes or host corrections separately from persistent control.
3. Check map overlay alignment on every map, different zoom/window sizes, and flipped Skeld. The inherited fanmade map-fit calculation needs visual validation against the current game.
4. Check hiding over chat/admin, scene resets, and meeting snapshots resuming after exile.
5. Save/reload settings; test autokill cooldown, meeting/dead-player behavior, and panic mode.
6. Verify official toasts, Influencer, ESP, multi-selection, and anti-exploit features.

Existing fanmade profiles may be copied into the official SickoMenu/sicko-config folder after backing them up. Older chat-preset formats may need recreation in the official UI.
