# Airship Electrical control: evidence and limits

Reviewed 2026-10-09 against the supplied Finnish research PDF, public project sources, and the installed game's IL2CPP metadata and map assets.

## What the implementation uses

The Airship maze is the `ElectricalDoors` system at `SystemTypes.Decontamination` (18), separate from ordinary Kitchen/Brig doors and the lights sabotage system. Its twelve `StaticDoor` references expose `SetOpen` and an `IsOpen` field. The installed metadata exposes `Serialize(MessageWriter, bool)`, `Deserialize(MessageReader, bool)` and `UpdateSystem(PlayerControl, MessageReader)`. `UpdateSystem` shares the empty-method RVA with `Deteriorate`; normal repair commands are not the useful control path.

Host changes set the native system's dirty flag. The opt-in non-host experiment invokes the current build's serializer, framed as GameData -> ShipStatus Data -> system 18. No integer encoding is guessed, no old packed/fixed format is copied, and no nested-message bypass is used. Only the selected local door is changed before serialization; remaining door states come from the live system. A successful send call is not a server acknowledgement or proof of other clients' state.

The DLL does not persist the experimental option across scene changes or automatically retry it. Meetings, panic, scene changes, invalid metadata and stale generations discard queued commands. Losing host authority requires the explicit experimental option before a queued command can apply.

## Server evidence

- [Waterway Room.ts at 75f3bb1](https://github.com/SkeldJS/Waterway/blob/75f3bb100ccfad29579368ebf817d473047d07f5/src/Room.ts): `handleDataMessage` finds an existing object, parses/handles its data and returns true. `handleMessagesAndGetNotCanceled` retains true results for forwarding. No host check is visible in this specific handler. This is a hypothesis about permissive deployments, not an end-to-end validation of every ingress path, plugin, protocol version or installed server.
- [Impostor InnerShipStatus](https://github.com/Impostor/Impostor/blob/master/src/Impostor.Server/Net/Inner/Objects/ShipStatus/InnerShipStatus.cs): host and broadcast validation precede system deserialization. A regular non-host ShipStatus update is not expected to pass default validation.
- [Hydra v1.9.0 release notes](https://github.com/MrDiamond64/Hydra/releases/tag/v1.9.0): the project reports that the historical broad bypass was patched at its root cause. It supplies no evidence that this Electrical experiment currently works on official servers.

The research report explicitly found no verified current official-server bypass. Official backend behavior cannot be inferred from an open-source replacement. This build contains an experiment, not a verified authority bypass.

## Required multiplayer validation

Use a controlled lobby with a host, the non-host test client, and an unmodified observer. Record the exact game version and server implementation/configuration. First verify normal host control on one door. Then enable the experimental option and change one door as non-host. The observer must confirm which door changed, whether the other eleven were preserved, and whether the host later restores it. Record rejected/disconnected, local-only, remotely visible then corrected, or remotely visible and persistent as separate outcomes.

No multiplayer results have been recorded for this feature. Automated tests cover target isolation, authority/opt-in gating, stale generations, index bounds and pin timing; they do not establish server acceptance or remote visibility.
