# Events & Scripting

`Event` nodes in a `.dialogue` graph fire named events that scripts react to.
This is how dialogue branches into game-state changes — combat triggers,
quest updates, scene transitions, audio cues, anything.

## The mechanism

```json
{
    "id":           "rude_ending",
    "type":         "event",
    "eventName":    "GuardAttacks",
    "jumpTargetId": "rude_ending_line"
}
```

When the runner advances onto an `Event` node, it:

1. Calls `OnEvent(eventName)` synchronously, which:
   - Fires the per-runner `OnDialogueEvent` signal with `eventName` as the arg.
   - Fires the global `Dialogue.OnEvent` dispatcher with `(eventName, dialogueAssetName)`.
2. Continues to `jumpTargetId` (typically a `line` node that says something
   matching the new state — the guard's "GUARDS!" reaction line).

`Event` nodes have no UI — the player never "sees" them. Style the
follow-up line with the appropriate portrait and text instead.

## Two ways to listen

### Per-runner — script attached to a node that knows the runner

Use this when only one specific dialogue's events matter to the script.

```lua
function myScript:Start()
    self.runner = self:GetWorld():FindNode("Runner")
    self.runner:ConnectSignal("OnDialogueEvent", self, function(self, eventName)
        if eventName == "GuardAttacks" then
            self:OnCombatTriggered()
        elseif eventName == "PlayerEntered" then
            self:OnGatesOpened()
        end
    end)
end

function myScript:Stop()
    if self.runner ~= nil then
        self.runner:DisconnectSignal("OnDialogueEvent", self)
    end
end
```

### Global — script anywhere

Use this for systems that react to events from any conversation: audio,
analytics, logging, achievements.

```lua
function audioSystem:Start()
    self.eventId = Dialogue.OnEvent("*", function(name, assetName)
        Log.Debug(string.format("event '%s' from %s", name, assetName))
        if name == "PlayerEntered" then
            Audio:Play("AmbientCity")
        end
    end)
end

function audioSystem:Stop()
    if self.eventId ~= nil then
        Dialogue.OffEvent(self.eventId)
        self.eventId = nil
    end
end
```

`"*"` matches every event. Subscribe to a specific name to filter:

```lua
local id = Dialogue.OnEvent("OpenDoor", function(name, assetName)
    DoorSystem:OpenByName(assetName)
end)
```

## End-state pattern

The most common use is "this dialogue ended this way." Define one event
per ending, fire it just before the final line:

```json
{ "id": "good_ending",      "type": "event", "eventName": "PlayerEntered",
  "jumpTargetId": "good_ending_line" },
{ "id": "good_ending_line", "type": "line", "speaker": "guard",
  "portrait": "Guard_Happy",
  "text": "Welcome to Selapang.",
  "jumpTargetId": "end" },

{ "id": "bribe_ending",      "type": "event", "eventName": "GuardBribed",
  "jumpTargetId": "bribe_ending_line" },
{ "id": "bribe_ending_line", "type": "line", "speaker": "guard",
  "portrait": "Guard_Idle",
  "text": "Be gone before I change my mind.",
  "jumpTargetId": "end" },

{ "id": "attack_ending",      "type": "event", "eventName": "GuardAttacks",
  "jumpTargetId": "attack_ending_line" },
{ "id": "attack_ending_line", "type": "line", "speaker": "guard",
  "portrait": "Guard_Attack",
  "text": "Then you have chosen poorly! TO ARMS!",
  "jumpTargetId": "end" }
```

The script then routes:

```lua
function questSystem:OnDialogueEvent(eventName)
    if eventName == "PlayerEntered" then
        self:CompleteQuest("kingdom_arrival_peaceful")
        Gates:Open()
    elseif eventName == "GuardBribed" then
        self:CompleteQuest("kingdom_arrival_bribed")
        Reputation:Adjust(-1)
        Gates:Open()
    elseif eventName == "GuardAttacks" then
        self:FailQuest("kingdom_arrival_peaceful")
        Combat:Begin("guard_squad")
    end
end
```

## Combining events with variables

Use `set_variable` nodes to track choices made during the conversation;
read them in `OnDialogueEvent` to vary the reaction:

```json
{ "id": "merchant_path", "type": "line", ...
  "variableOp": { "op": "set", "var": "guard_friendly", "value": true },
  ... }
```

```lua
function questSystem:OnDialogueEvent(eventName)
    local friendly = self.runner:GetBool("guard_friendly", false)
    if eventName == "PlayerEntered" and friendly then
        Reputation:Adjust(+1)
    end
end
```

## Global flags for cross-system reactions

Variables set on a runner are scoped to that conversation. For game-wide
state (other NPCs should know the player bribed the gate guard), use the
global store:

```lua
-- in the dialogue's reaction handler
function gateScript:OnDialogueEvent(eventName)
    if eventName == "GuardBribed" then
        Dialogue.SetGlobalBool("kingdom_bribed_entry", true)
    end
end

-- in another NPC's dialogue or script later
local bribed = Dialogue.GetGlobalBool("kingdom_bribed_entry", false)
if bribed then
    Audio:Play("CityWhispersAboutBribery")
end
```

## OnLineChanged also gives you hooks

For non-event-driven reactions (per-line audio cues, camera shakes on
specific speakers, log every line spoken), use `OnLineChanged`:

```lua
runner:ConnectSignal("OnLineChanged", self, function(self, speakerId, text, locKey)
    Audio:PlayVoice(speakerId, locKey)
    Log.Debug(speakerId .. ": " .. text)
end)
```

`OnChoicesChanged`, `OnChoiceSelected`, `OnDialogueStarted/Stopped/Finished`
all fire the same way — see [`03-RuntimeAPI.md`](03-RuntimeAPI.md) for the
full signal table.

## Cleanup

Disconnect signals in your `Stop()`:

```lua
function myScript:Stop()
    if self.runner ~= nil then
        self.runner:DisconnectSignal("OnDialogueEvent", self)
        self.runner:DisconnectSignal("OnLineChanged",   self)
    end
    if self.globalEventId ~= nil then
        Dialogue.OffEvent(self.globalEventId)
        self.globalEventId = nil
    end
end
```

The addon's `OnUnload` clears the global event dispatcher's listener
list, so hot-reloading the addon won't leave dangling Lua refs even if
you forget to clean up. Per-runner signal connections die with the
runner node.

## A complete script example

The demo project's `Scripts/startTalking.lua` shows the full pattern:
gamepad-A start, choice-aware advance, end-state event router, optional
scene-node references for visible reactions (gates opening, arena
trigger, fade overlay). Read it for a working template.
