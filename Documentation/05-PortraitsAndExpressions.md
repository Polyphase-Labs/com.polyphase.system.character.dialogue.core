# Portraits & Expressions

Each line resolves to a portrait texture asset name. The runner caches the
resolved name on the current line; the widget loads the texture by name
via the engine's asset system and applies it to a wired `Quad` widget.

## Resolution order

For each line emitted by the runner:

1. **Per-node `portrait`** field on the line, if set.
2. **Speaker default** — the `portrait` field on the matching speaker in
   `graph.mSpeakers`, looked up by `node.speaker`.
3. **Empty** — the widget hides its `Portrait Quad`.

So you set a default per speaker (`Guard_Idle`) and override per line for
specific emotional beats (`Guard_Happy`, `Guard_Mad`).

## Defining speakers

```json
"speakers": [
    { "id": "guard",  "name": "Captain Pasukan", "portrait": "Guard_Idle" },
    { "id": "player", "name": "You" }
]
```

The `portrait` field is the texture asset name as it appears in the
project's asset browser (no extension — the engine's asset system
identifies it by name).

The `player` speaker above has no portrait — when the player has a line,
the Portrait Quad hides.

## Per-line overrides

```json
{
    "id": "stern_warning", "type": "line", "speaker": "guard",
    "portrait": "Guard_Mad",
    "text": "I will not warn you again."
}

{
    "id": "back_to_normal", "type": "line", "speaker": "guard",
    "portrait": "Guard_Idle",   // back to default look
    "text": "Move along."
}
```

Each `portrait` field is just a texture asset name. The runtime doesn't
care what the file format is — it goes through `api->LoadAsset(name)`
and casts the result to `Texture*`.

## Naming convention for "expressions"

There's no special expression system — just consistent asset naming:

```
Guard_Idle      Guard_Happy     Guard_Mad       Guard_Attack
King_Idle       King_Sad        King_Furious
Princess_Default  Princess_Surprised  Princess_Smiling
```

Author each as a separate Texture asset in the editor, give them
matching base-name + emotion-suffix names, and reference them from
nodes / speakers.

## Wiring the widget

In the inspector on `DialogueBoxWidget`:

- **Portrait Quad** → drag a `Quad` widget from your DialogueBox tree.

That's it. Every `OnLineChanged` will:

1. Resolve the portrait via the runner.
2. If empty → hide the Quad.
3. Else → `api->LoadAsset(name)` → cast to `Texture*` → `Quad::SetTexture`.

If the asset isn't found or isn't a Texture, the widget logs a warning
and hides the Quad.

## Reading the portrait from Lua

```lua
runner:ConnectSignal("OnLineChanged", self, function(self)
    local p = runner:GetCurrentPortrait()
    Log.Debug("portrait now: " .. p)
end)

-- or pull on demand
local current = runner:GetCurrentPortrait()
```

Useful for scripts that drive their own non-widget UI, or for syncing
portraits to a separate avatar mesh.

## What about missing assets?

If a line says `"portrait": "Guard_Cackle"` but no `Guard_Cackle.png`
exists, the widget logs:

```
DialogueBoxWidget 'DialogueBoxWidget': portrait asset 'Guard_Cackle' not found
```

…and hides the portrait. The dialogue continues normally. The validator
doesn't catch missing portrait assets at edit time — that's an asset-system
question, not a graph-integrity one.

## Gotchas

- **Texture asset names are case-sensitive on consoles.** Stick to one
  case convention across your portraits.
- **The widget caches `Portrait Quad` at `Start()`** but resolves the
  texture every line change. Recreating the Quad mid-conversation
  doesn't break anything until you call `SetRunner` again.
- **Widget inheritance won't work** — `Portrait Quad` must be a `Quad`
  widget specifically. The widget checks via `GetClassName() == "Quad"`.

## A complete demo

`Assets/Dialogue/Sample.dialogue` (the castle-gate scenario) walks the
guard through `Guard_Idle` → `Guard_Happy` (welcoming a polite merchant)
→ `Guard_Mad` (confronting rude or bribing player) → `Guard_Attack` (the
player draws blood). Same speaker throughout — only the `portrait` field
on each line changes.

## Future work

Voice-line playback driven by the speaker's `voice` field is reserved in
the schema but not yet wired in the MVP runtime. When it lands, the
runner will fire `PlaySound2D` on each line change with the resolved
voice asset.
