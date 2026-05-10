# Polyphase Dialogue Core

A console-safe runtime dialogue system for the Polyphase engine. Ships as a
native addon — drop it into a project's `Packages/` directory and the editor
discovers it on next open.

| What you get | Where it lives |
|---|---|
| `DialogueAsset` — branching dialogue graphs imported from `.dialogue` JSON files | `Source/Assets/` |
| `DialogueRunner3D` — `Node3D` that drives a conversation and emits signals | `Source/Nodes/` |
| `DialogueBoxWidget` — `Widget` that renders the current line, portraits, choices, and a press-to-continue indicator | `Source/Widgets/` |
| Lua API — instance methods on `DialogueRunner3D` plus a global `Dialogue.*` module | `Source/Lua/` |
| Editor preview window — step through a dialogue without putting it in a scene | `Source/Editor/` |
| Editor validator — check graphs for missing speakers, unreachable nodes, broken jumps | `Source/Editor/` |

## Quick example

```json
// Assets/Dialogue/Greet.dialogue
{
    "start": "hello",
    "speakers": [
        { "id": "guard", "name": "Guard", "portrait": "Guard_Idle" }
    ],
    "nodes": [
        { "id": "hello", "type": "line", "speaker": "guard",
          "portrait": "Guard_Happy",
          "text": "Welcome to the kingdom!",
          "jumpTargetId": "end" },
        { "id": "end", "type": "end" }
    ]
}
```

```lua
-- in any Lua script attached to a node
runner = self:GetWorld():FindNode("Runner")     -- a DialogueRunner3D
runner:StartDialogue()
```

The `DialogueBoxWidget` (wired to the same runner via its `Runner Path`
property) shows the speaker, line, and portrait automatically.

## Read the docs

Start with [`Documentation/01-GettingStarted.md`](Documentation/01-GettingStarted.md).
Full table of contents in [`Documentation/README.md`](Documentation/README.md).

## Companion addon

The optional `com.polyphase.system.character.dialogue.ink` addon adds an
editor-time importer that converts compiled Ink JSON (`*.ink.json`) into a
`.dialogue` sidecar file. Consoles still ship only the cooked `DialogueAsset`
so no Ink runtime is required.

## Scope

This is the MVP runtime: branching choices, conditions, simple variables,
events, save/load, editor preview, console-safe (no RTTI, no exceptions). The
following features from the design spec are deferred:

- Full visual graph editor (basic asset inspector + interactive preview ship today)
- NodeGraph dialogue nodes
- Yarn / full Ink runtime bridge
- Console QA pass (the runtime is exception-free by construction but isn't
  yet validated on 3DS / Wii / GameCube hardware)

## Apropos

`Tuan Pasukan` (Captain), `Kerajaan` (Kingdom), `Selapang` — the demo
content's flavour. Replace it with your own.
