# Airship Electrical control: evidence and limits

Reviewed 2026-10-09 against the supplied Finnish research PDF, public project sources, and the installed game's IL2CPP metadata and map assets.

## Observed result and current behavior

The user reported that the experimental non-host click kicked the alt from an official server. The exact disconnect reason and packet logs were not provided, so this establishes a failed attempt, not the precise server check. The direct state sender and its opt-in UI have now been removed. Electrical controls are host-only; non-hosts can view the live map. No alternate method satisfying official server + only non-host client modded has been verified.

## What the implementation uses

The Airship maze is the `ElectricalDoors` system at `SystemTypes.Decontamination` (18), separate from ordinary Kitchen/Brig doors and the lights sabotage system. Its twelve `StaticDoor` references expose `SetOpen` and an `IsOpen` field. The installed metadata exposes `Serialize(MessageWriter, bool)`, `Deserialize(MessageReader, bool)` and `UpdateSystem(PlayerControl, MessageReader)`. `UpdateSystem` shares the empty-method RVA with `Deteriorate`; normal repair commands are not the useful control path.

Host changes set the native system's dirty flag. The removed experiment invoked the current build's serializer, framed as GameData -> ShipStatus Data -> system 18. It did not guess integer encoding or use the historical nested-message bypass. A successful send call was not a server acknowledgement or proof of other clients' state.

Current code has no non-host state sender or experimental toggle. Commands are checked for current host authority both when queued and when processed. Meetings, panic and scene changes discard pending actions.

## Server evidence

- [Waterway Room.ts at 75f3bb1](https://github.com/SkeldJS/Waterway/blob/75f3bb100ccfad29579368ebf817d473047d07f5/src/Room.ts): `handleDataMessage` finds an existing object, parses/handles its data and returns true. `handleMessagesAndGetNotCanceled` retains true results for forwarding. No host check is visible in this specific handler. This is a hypothesis about permissive deployments, not an end-to-end validation of every ingress path, plugin, protocol version or installed server.
- [Impostor InnerShipStatus](https://github.com/Impostor/Impostor/blob/master/src/Impostor.Server/Net/Inner/Objects/ShipStatus/InnerShipStatus.cs): host and broadcast validation precede system deserialization. A regular non-host ShipStatus update is not expected to pass default validation.
- [Hydra v1.9.0 release notes](https://github.com/MrDiamond64/Hydra/releases/tag/v1.9.0): the project reports that the historical broad bypass was patched at its root cause. It supplies no evidence that this Electrical experiment currently works on official servers.

The research report explicitly found no verified current official-server bypass. Official backend behavior cannot be inferred from an open-source replacement. The failed experiment is no longer present in this build.

## Remaining validation

Host Electrical controls and the map still need in-game validation. Non-host Electrical clicks must not mutate doors or send ShipStatus state. Automated tests cover authority gating, target isolation, stale generations, index bounds and pin timing.

## Separate Polus client regression

The supplied official utility used `RpcUpdateSystem(Doors, doorId | 64)` for each matching door and `RpcCloseDoorsOfType(room)` to close them, with no host-only gate. The paired-door rewrite had introduced a host gate; that regression is now removed by a separate client branch using those same ordinary RPCs. The host's direct state synchronization is never called as non-host.

Open targets the two actual physical door references of the selected chamber. Since all four Polus physical doors share one Room, Close takes a snapshot, closes the shared room, and requests reopening of non-selected doors that were previously open. Hard-pinned peer chambers are not reopened. This can create a brief close on the other chamber and cannot cancel a native host-controlled decon cycle. Non-host hard pins pulse every 50 ms even when the local pair is closed, wait for the RPC queue to drain, and immediately suppress local reopen attempts. Soft pins retain the 1.5-second opening window.

The user did not check the old behavior from another client's screen. Consequently the restored client request path is not claimed to be proven global control; it requires an unmodified observer test, particularly during active cycles and when the other chamber is open.
