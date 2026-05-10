# Getting Started

End-to-end walkthrough: drop the addon into a project, author a `.dialogue`
file, build a scene, drive it from a script.

## 1. Install

Copy `com.polyphase.system.character.dialogue.core` into your project's
`Packages/` directory. In the editor:

1. **Tools → Addons → Regenerate Native Addon Dependencies**
2. **Tools → Addons → Reload Native Addons**

The console should log:

```
dialogue.core addon loaded
```

If you also have the Ink importer addon, you'll see `dialogue.ink addon loaded`.

## 2. Author a .dialogue file

Create `Assets/Dialogue/Hello.dialogue`:

```json
{
    "start": "intro",
    "speakers": [
        { "id": "guard", "name": "Guard", "portrait": "Guard_Idle" }
    ],
    "nodes": [
        { "id": "intro", "type": "line", "speaker": "guard",
          "text": "Halt! State your business.",
          "choices": [
            { "id": "polite", "text": "Just passing through.", "target": "polite_resp" },
            { "id": "rude",   "text": "Mind your own affairs.", "target": "rude_resp" }
          ]
        },
        { "id": "polite_resp", "type": "line", "speaker": "guard",
          "portrait": "Guard_Happy",
          "text": "Move along then.",
          "jumpTargetId": "end" },
        { "id": "rude_resp", "type": "event", "eventName": "GuardAggro",
          "jumpTargetId": "rude_resp_line" },
        { "id": "rude_resp_line", "type": "line", "speaker": "guard",
          "portrait": "Guard_Mad",
          "text": "Bold words. GUARDS!",
          "jumpTargetId": "end" },
        { "id": "end", "type": "end" }
    ]
}
```

Drop the file into the asset browser. The editor's importer recognizes
`.dialogue` and creates a `DialogueAsset`.

The `.dialogue` extension is the editor's import key — the file is JSON.
For syntax-highlighted authoring in your code editor, point the
`.dialogue` extension at JSON.

> Full schema: [`02-DialogueFormat.md`](02-DialogueFormat.md)

## 3. Add textures for the portraits

Import these as Texture assets in the editor with these exact names:

```
Guard_Idle    (default look)
Guard_Happy   (smiling)
Guard_Mad     (angry)
```

You can use any image format the editor supports. The runtime looks up the
texture by name via the engine's asset system.

> Portrait system, expressions, and per-line overrides:
> [`05-PortraitsAndExpressions.md`](05-PortraitsAndExpressions.md)

## 4. Build the scene

In the editor, build a scene with this minimum shape:

```
Root
├── Runner          (DialogueRunner3D)         — drag your DialogueAsset into "Dialogue Asset"
└── UI                 (Canvas)
    └── DialogueBox (DialogueBoxWidget)        — wire references below
        ├── Speaker        (Text)              — set Font; reads from "Speaker Child" property
        ├── Body           (Text)              — set Font
        ├── Portrait       (Quad)              — drag into "Portrait Quad" property
        ├── ContinueArrow  (Quad)              — drag into "Continue Indicator" property
        ├── Choice0        (Button)            — drag into "Choices" array, slot 0
        ├── Choice1        (Button)            — drag into "Choices" array, slot 1
        └── ...                                   add as many slots as your max-choice node needs
```

Each `Button` should have a `Font` set on its inner Text (via
`Button.SetTextString` or its inspector). The widget hides choice slots
beyond the current node's choice count, so authoring fewer slots than the
worst case logs a warning but doesn't crash.

> Widget properties + scene shape: [`04-DialogueBoxWidget.md`](04-DialogueBoxWidget.md)

## 5. Drive it from Lua

```lua
-- Scripts/talk_demo.lua
talk_demo = {}

function talk_demo:Start()
    self.runner = self:GetWorld():FindNode("Runner")
    self.runner:StartDialogue()
end

function talk_demo:Tick(dt)
    -- The DialogueBoxWidget handles A-press input on its own (number keys
    -- 1-9 for choices in keyboard mode; gamepad A on focused buttons). For
    -- console-only setups where the player presses a gamepad button to
    -- both *start* and *advance* the conversation, see startTalking.lua
    -- in the demo project.
end

return talk_demo
```

Attach the script to any node in the scene. Press play.

> Full Lua API + signals: [`03-RuntimeAPI.md`](03-RuntimeAPI.md)

## 6. React to events from the script

```lua
function talk_demo:Start()
    self.runner = self:GetWorld():FindNode("Runner")
    self.runner:ConnectSignal("OnDialogueEvent", self, function(self, eventName)
        if eventName == "GuardAggro" then
            Log.Warning("Guard is hostile!")
            -- spawn enemies, fade screen, transition scene, etc.
        end
    end)
    self.runner:StartDialogue()
end
```

`Event` nodes in the dialogue fire `OnDialogueEvent`. Use this to wire
end-state reactions (combat, scene transitions, quest updates).

> Event-driven scripting patterns: [`06-EventsAndScripting.md`](06-EventsAndScripting.md)

## 7. Step through it without playing the scene

**Tools → Dialogue → Open Preview** — type the asset name, click `Start`,
walk through the choices interactively. The preview runs an isolated runner
so it doesn't affect any in-scene state.

> [`07-EditorPreview.md`](07-EditorPreview.md)

---

## Where to next

- **Author conversations**: [Dialogue Format](02-DialogueFormat.md)
- **Style the UI**: [DialogueBoxWidget](04-DialogueBoxWidget.md)
- **Wire events**: [Events & Scripting](06-EventsAndScripting.md)
- **Save game state**: [Save / Load](08-SaveLoad.md)
- **Hit a bug**: [Troubleshooting](10-Troubleshooting.md)
