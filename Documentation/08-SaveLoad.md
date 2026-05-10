# Save / Load

The `DialogueRunner3D` exposes two save paths:

1. **Engine scene-save integration** — automatic, used by the engine when
   the scene saves. Round-trips `mCurrentNodeId`, `mRunning`, and the
   per-runner variable store.
2. **Slot-style state I/O** — explicit `SaveDialogueState` /
   `LoadDialogueState` for game saves: serialise to a `std::vector<uint8_t>`
   you can write to disk, send over network, etc.

## Scene serialization

Every `DialogueRunner3D` overrides `SaveStream` / `LoadStream`:

```cpp
void DialogueRunner3D::SaveStream(Stream& stream, Platform platform)
{
    Node3D::SaveStream(stream, platform);
    mRunner.SaveStream(stream);          // mRunning + mCurrentNodeId
    mVariableStore.SaveStream(stream);   // per-runner variables
}

void DialogueRunner3D::LoadStream(Stream& stream, Platform platform, uint32_t version)
{
    Node3D::LoadStream(stream, platform, version);
    mRunner.LoadStream(stream);
    mVariableStore.LoadStream(stream);
}
```

When you save a scene mid-conversation, the runner's "currently on node X"
position is preserved. On reload, the next time the widget's `Start()` runs
it sees `IsDialogueRunning() == true` and immediately pulls the current
speaker / text / portrait via the runner's getters — no re-emit of
`OnLineChanged` is needed.

This works automatically for editor `Save Scene` and runtime PIE
session-save. Nothing for you to wire.

## Slot-style state I/O

For arbitrary save points (a save-game system not tied to scene saves):

### C++

```cpp
DialogueRunner3D* runner = ...;

// snapshot
std::vector<uint8_t> snapshot;
runner->SaveDialogueState(snapshot);
SaveBlob("savegame_dialogue.bin", snapshot);

// restore
std::vector<uint8_t> snapshot = LoadBlob("savegame_dialogue.bin");
if (runner->LoadDialogueState(snapshot))
{
    // runner is now at the previously-saved node, with the previously-saved
    // variables. The widget will re-pull on next Start.
}
```

### Lua

The Lua bindings don't currently expose `SaveDialogueState` /
`LoadDialogueState` (they return `std::vector<uint8_t>` which doesn't
trivially marshal). If you need this from Lua, drive it via your own C++
helper or use the global variable store as the persistence boundary
instead:

```lua
-- before save: snapshot relevant variables
local rep    = runner:GetInt("rep", 0)
local met    = runner:GetBool("met_guard", false)
local nodeId = runner:GetCurrentNodeId()  -- TODO: not currently exposed in Lua;
                                          -- add a binding if you need this

-- to a file or wherever
SaveGame:Set("dialogue.rep",    rep)
SaveGame:Set("dialogue.met",    met)
SaveGame:Set("dialogue.nodeId", nodeId)

-- on load
runner:SetInt("rep", SaveGame:Get("dialogue.rep", 0))
runner:SetBool("met_guard", SaveGame:Get("dialogue.met", false))
runner:StartDialogueAtNode(SaveGame:Get("dialogue.nodeId", "intro"))
```

## What survives a round-trip

| Survives | Notes |
|---|---|
| ✅ Current node id | The runner's "I'm at node X" position |
| ✅ Running flag | True iff a conversation was active |
| ✅ Per-runner variables (bool/int/float/string) | All stored values |
| ❌ Available choices (cached) | Re-derived from the current node on next signal emit / pull |
| ❌ Speaker name / text / portrait (cached strings) | Re-resolved from current node |
| ❌ Signal listeners | Connections aren't serialized — re-wire them in `Start()` |
| ❌ Global variable store | Save / load this separately if you want it persistent |
| ❌ Event listeners (`Dialogue.OnEvent`) | Same — re-register at startup |
| ❌ DialogueAsset reference | The runner's `Dialogue Asset` property is a regular `AssetRef`; the engine handles its serialization separately as part of the standard property save |

## Global state is separate

`Dialogue.SetGlobalBool` writes to the addon-wide global store, which is
**not** serialised by `DialogueRunner3D`. If you want game-wide flags to
persist:

```lua
-- on save
SaveGame:Set("kingdom_bribed", Dialogue.GetGlobalBool("kingdom_bribed", false))

-- on load
Dialogue.SetGlobalBool("kingdom_bribed", SaveGame:Get("kingdom_bribed", false))
```

Or write a small C++ helper that walks `DialogueAddon::DialogueManager::GetGlobalStore()`
and serialises each entry.

## Hot-reload behaviour

When the addon is hot-reloaded:

1. `OnUnload` calls `DialogueManager::StopAll()` — every active runner
   stops cleanly, fires `OnDialogueStopped`, and clears its current state.
2. `DialogueManager::Clear()` empties the global variable store and the
   live-runner tracking set.
3. `DialogueEventDispatcher::Clear()` releases stored Lua-side
   `ScriptFunc` references.

All of this happens before the DLL is freed, so there's no chance of a
post-FreeLibrary callback into dangling code. After reload, runners
re-register in `Create()` — no manual rewiring needed for in-scene state.

If a save was active across the reload, the in-memory runner state is
gone, but the scene-saved version on disk is unaffected.

## Versioning

`DialogueRunner3D::SaveStream` writes via `Node3D::SaveStream`, which
includes the engine's version number. The slot-format API
(`SaveDialogueState`) is a raw byte blob with no internal version — if
you change `mRunner` or `mVariableStore`'s on-wire layout, bump a
version byte at the start of your own snapshot wrapper.

The on-disk `DialogueAsset` format (separately) has its own version
stored in `mGraph.mVersion` plus a magic-anchored trailer for
forward-compatible field additions. See [`02-DialogueFormat.md`](02-DialogueFormat.md).
