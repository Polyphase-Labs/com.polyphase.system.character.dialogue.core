# Runtime API

`DialogueRunner3D` is the conversation driver — a `Node3D` that owns a
`DialogueRunner` (pure runtime class), its own variable store, and the
event signal surface. Drop one in a scene, set its `Dialogue Asset`, and
talk to it from Lua / C++.

## C++ surface

```cpp
class DialogueRunner3D : public Node3D
{
public:
    // Asset binding
    void           SetDialogueAsset(DialogueAsset* asset);
    DialogueAsset* GetDialogueAsset() const;

    // Lifecycle
    void StartDialogue();
    void StartDialogueAtNode(const std::string& nodeId);
    void StopDialogue();
    bool IsDialogueRunning() const;

    // Flow
    void ContinueDialogue();
    void ChooseDialogueOption(uint32_t index);
    void ChooseDialogueOptionById(const std::string& choiceId);

    // Queries (refreshed in-place by OnLineChanged / OnChoicesChanged)
    const std::string& GetCurrentSpeaker() const;       // raw id
    const std::string& GetCurrentSpeakerName() const;   // resolved display name
    const std::string& GetCurrentText() const;
    const std::string& GetCurrentLocKey() const;
    const std::string& GetCurrentPortrait() const;      // resolved per-line / speaker default
    uint32_t           GetNumChoices() const;
    const std::string& GetChoiceText(uint32_t i) const;
    const std::string& GetChoiceId(uint32_t i) const;

    // Per-runner variables (fall through to global store when "Use Global Store" is true)
    void        SetBool  (const std::string&, bool);
    void        SetInt   (const std::string&, int32_t);
    void        SetFloat (const std::string&, float);
    void        SetString(const std::string&, const std::string&);
    bool        GetBool  (const std::string&, bool        def = false) const;
    int32_t     GetInt   (const std::string&, int32_t     def = 0) const;
    float       GetFloat (const std::string&, float       def = 0.0f) const;
    std::string GetString(const std::string&, const std::string& def = "") const;

    // Slot-style state I/O
    void SaveDialogueState(std::vector<uint8_t>& outBytes) const;
    bool LoadDialogueState(const std::vector<uint8_t>& bytes);
};
```

## Lua API

The metatable name is `"DialogueRunner3D"` (matches the `DECLARE_NODE`
class name). Inherits from `Node3D`'s Lua metatable, so all `Node3D`
methods (`SetPosition`, `GetWorld`, `ConnectSignal`, ...) are available.

### Asset binding

```lua
runner:SetDialogueAsset(asset)   -- asset is a DialogueAsset, or nil to clear
runner:GetDialogueAsset()        -- returns Asset (cast in Lua via CHECK_ASSET)
```

### Lifecycle

```lua
runner:StartDialogue()                          -- start at the asset's "start" node
runner:StartDialogueAtNode("specific_node_id")
runner:StopDialogue()
runner:IsDialogueRunning()  -- returns bool
```

### Flow

```lua
runner:ContinueDialogue()         -- advance when no choices are pending
runner:ChooseDialogueOption(0)    -- 0-indexed; targets the *available* choice list
                                  -- (choices filtered out by failed conditions don't count)
```

`ContinueDialogue` is a no-op when choices are pending — the player must
pick one. `ChooseDialogueOption` with an out-of-range index logs an
`OnDialogueError` and is otherwise a no-op.

### Queries

```lua
runner:GetCurrentSpeaker()      -- raw speaker id, e.g. "guard"
runner:GetCurrentSpeakerName()  -- resolved display name, e.g. "Captain Pasukan"
runner:GetCurrentText()
runner:GetCurrentLocKey()
runner:GetCurrentPortrait()     -- resolved texture asset name

runner:GetNumChoices()          -- 0..N (filtered by conditions)
runner:GetChoiceText(0)
runner:GetChoiceId(0)           -- the choice's `id` field from the .dialogue file
```

### Variables

```lua
runner:SetBool("met_guard", true)
runner:GetBool("met_guard", false)   -- second arg is default if not set

runner:SetInt("rep", 5)
runner:GetInt("rep", 0)

runner:SetFloat("affinity", 0.5)
runner:GetFloat("affinity", 0.0)

runner:SetString("flag", "x")
runner:GetString("flag", "")
```

When `Use Global Store` is true on the runner (default), `GetX` falls
through to the addon-wide global store if the per-runner store doesn't
have the value. `SetX` always writes to the per-runner store.

## Signals

Connect via the standard `Node:ConnectSignal(name, listener, fn)` pattern.

| Signal | Args |
|---|---|
| `OnDialogueStarted` | `(none)` |
| `OnDialogueStopped` | `(none)` |
| `OnDialogueFinished` | `(none)` |
| `OnLineChanged` | `(speakerId, text, locKey)` — query the runner for the resolved name and portrait |
| `OnChoicesChanged` | `(numChoices)` |
| `OnChoiceSelected` | `(choiceId, choiceText)` |
| `OnDialogueEvent` | `(eventName)` |
| `OnDialogueError` | `(message)` |

```lua
runner:ConnectSignal("OnLineChanged", self, function(self, speakerId, text, locKey)
    Log.Debug(string.format("[%s] %s", speakerId, text))
end)

runner:ConnectSignal("OnDialogueEvent", self, function(self, eventName)
    if eventName == "GuardAggro" then
        Combat:BeginEncounter()
    end
end)
```

`Stopped` and `Finished` differ only in cause: `Stopped` fires when
something calls `StopDialogue` mid-conversation; `Finished` fires when an
`end` node is hit naturally (or a node with no jump target settles into a
"natural end").

## Global Dialogue module

A separate Lua table for cross-conversation state. Useful for game-wide
flags ("met_guard" should persist across multiple dialogues) and for
listening to dialogue events globally.

### Variable I/O

```lua
Dialogue.SetGlobalBool("met_guard", true)
Dialogue.GetGlobalBool("met_guard", false)

Dialogue.SetGlobalInt(   "rep", 10)
Dialogue.GetGlobalInt(   "rep", 0)

Dialogue.SetGlobalFloat( "affinity", 0.5)
Dialogue.GetGlobalFloat( "affinity", 0.0)

Dialogue.SetGlobalString("kingdom", "Selapang")
Dialogue.GetGlobalString("kingdom", "")
```

The global store is shared by every `DialogueRunner3D` whose `Use Global
Store` property is true.

### Event subscription

```lua
local id = Dialogue.OnEvent("OpenDoor", function(eventName, dialogueAssetName)
    DoorSystem:Open(eventName)
end)

-- subscribe to every event with "*"
local idAll = Dialogue.OnEvent("*", function(eventName, dialogueAssetName)
    Log.Debug("event " .. eventName .. " from " .. dialogueAssetName)
end)

-- later
Dialogue.OffEvent(id)
```

The event dispatcher is hot-reload-safe — addon `OnUnload` clears every
listener before the DLL is freed, so stored Lua references can't dangle.

## C++ access from a custom Node

If you write your own node that owns or talks to a `DialogueRunner3D`:

```cpp
#include "Nodes/DialogueRunner3D.hxx"

void MyNode::Tick(float dt)
{
    if (mRunner != nullptr && mRunner->IsDialogueRunning())
    {
        // pull state, drive UI, etc.
    }
}
```

For cross-DLL safety in your own widgets, prefer string class-name checks
(`Node::GetClassName()`) over `GetType() == X::GetStaticType()` — the
TypeId comparison can fail across the addon/engine module boundary.

## Save / load

```cpp
std::vector<uint8_t> snapshot;
runner->SaveDialogueState(snapshot);   // serializes mCurrentNodeId, mRunning, mVariableStore

// later, possibly across PIE sessions:
runner->LoadDialogueState(snapshot);   // restores. Does NOT re-emit OnLineChanged
                                       // — the widget re-pulls via getters on its next Start.
```

See [`08-SaveLoad.md`](08-SaveLoad.md) for round-trip semantics and
`runner.SaveStream` / `runner.LoadStream` (the engine's scene-save path).
