# Polyphase Dialogue Core — Documentation

Read in order if you're new to the addon. Each doc is self-contained for
reference once you've found your way around.

| # | Doc | What's covered |
|---|---|---|
| 01 | [Getting Started](01-GettingStarted.md) | Install, scene setup, first conversation |
| 02 | [Dialogue Format](02-DialogueFormat.md) | `.dialogue` JSON schema, every field |
| 03 | [Runtime API](03-RuntimeAPI.md) | C++ + Lua API of `DialogueRunner3D`, signals, global `Dialogue.*` module |
| 04 | [DialogueBoxWidget](04-DialogueBoxWidget.md) | Widget properties, scene shape, typewriter, continue indicator |
| 05 | [Portraits & Expressions](05-PortraitsAndExpressions.md) | Speaker portraits, per-line overrides, named expressions |
| 06 | [Events & Scripting](06-EventsAndScripting.md) | `Event` nodes, `OnDialogueEvent`, reaction patterns |
| 07 | [Editor Preview](07-EditorPreview.md) | Tools → Dialogue → Open Preview, asset inspector, validator |
| 08 | [Save / Load](08-SaveLoad.md) | Persisting runner state across sessions |
| 09 | [Platform Support](09-PlatformSupport.md) | Console safety, what works where |
| 10 | [Troubleshooting](10-Troubleshooting.md) | Common issues + diagnostic patterns |
| 11 | [Architecture](11-Architecture.md) | How the pieces fit (Asset → Runner → Node → Widget) |

## Companion addon

The Ink importer (`com.polyphase.system.character.dialogue.ink`) ships its
own docs in its own `Docs/` folder — read those if you want to convert
`.ink.json` content into `.dialogue` files automatically.

## Source layout

```
Source/
  Assets/           DialogueAsset, DialogueTypes (POD types), DialogueJson (parser/writer)
  Runtime/          DialogueRunner, VariableStore, addon-owned singletons (Manager, Events)
  Nodes/            DialogueRunner3D — the scene-graph driver
  Widgets/          DialogueBoxWidget — UI renderer
  Lua/              DialogueRunner3D + global Dialogue module bindings
  Editor/           Asset inspector, preview window, validator (#if EDITOR only)
  DialogueCoreAddon.cpp   Plugin entry, OnLoad / OnUnload, type registration
  EngineAPIAccess.hxx     Cached PolyphaseEngineAPI* accessor
```
