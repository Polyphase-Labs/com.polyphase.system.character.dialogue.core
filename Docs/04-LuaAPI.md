# Lua API

## DialogueRunner3D instance methods

```lua
runner:SetDialogueAsset(asset)
runner:GetDialogueAsset()

runner:StartDialogue()
runner:StartDialogueAtNode("node_id")
runner:StopDialogue()
runner:IsDialogueRunning()

runner:ContinueDialogue()
runner:ChooseDialogueOption(0)        -- 0-indexed

runner:GetCurrentSpeaker()
runner:GetCurrentText()
runner:GetCurrentLocKey()
runner:GetNumChoices()
runner:GetChoiceText(0)
runner:GetChoiceId(0)

runner:SetBool("met_guard", true)
runner:GetBool("met_guard", false)
runner:SetInt("rep", 5)
runner:GetInt("rep", 0)
runner:SetFloat("affinity", 0.5)
runner:GetFloat("affinity", 0.0)
runner:SetString("flag", "x")
runner:GetString("flag", "")
```

Signals are connected via `Node:ConnectSignal(name, listener, fn)` (engine API).

## Global Dialogue module

```lua
Dialogue.SetGlobalBool(name, value)
Dialogue.GetGlobalBool(name, default)
Dialogue.SetGlobalInt(name, value)
Dialogue.GetGlobalInt(name, default)
Dialogue.SetGlobalFloat(name, value)
Dialogue.GetGlobalFloat(name, default)
Dialogue.SetGlobalString(name, value)
Dialogue.GetGlobalString(name, default)

-- Subscribe to dialogue events globally. Returns a non-zero listener id.
-- Pass "*" to receive every event.
local id = Dialogue.OnEvent("OpenDoor", function(eventName, dialogueAssetName)
    -- ...
end)

Dialogue.OffEvent(id)
```

Per-runner variables are scoped; the global store is for cross-conversation
flags. `runner:GetBool` falls through to the global store when the variable
isn't set on the runner (and `Use Global Store` is true on the runner — the
default).
