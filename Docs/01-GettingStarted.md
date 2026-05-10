# Polyphase Dialogue Core — Getting Started

This addon provides a console-safe runtime dialogue system for Polyphase. It
ships with:

- `DialogueAsset` — graph asset type, imported from `.dialogue`.
- `DialogueRunner3D` — Node3D wrapper that drives a conversation.
- `DialogueBoxWidget` — auto-binding widget that renders the current line and choices.
- Lua API: `Dialogue.*` global module + `DialogueRunner3D` instance methods.
- Editor: dialogue inspector + interactive `Tools → Dialogue → Open Preview` window.

## Quick start

1. Drop a `.dialogue` file into the project's asset browser. The core
   importer creates a `DialogueAsset`.
2. Add a `DialogueRunner3D` to your scene. Set its `Dialogue Asset` property.
3. Add a `DialogueBoxWidget` under a `Canvas`. Create children named `Speaker`,
   `Body`, `Choices` (all `Text` widgets). Set the box's `Runner Path` to the
   relative path of the runner.
4. From a Lua script call `runner:StartDialogue()`.

The widget reads input via the engine's `IsKeyJustPressed` API and advances on
the configured key (default Space). Number keys 1..9 select choices.

## Console safety

The runtime runner, asset, variable store, and widget are exception-free and
RTTI-free. The Ink importer is editor-only — consoles ship only the cooked
`DialogueAsset` produced at editor time.
