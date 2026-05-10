# Architecture

How the pieces fit together. Read this if you're modifying the addon
itself or trying to understand why something is structured the way it is.

## Layered design

```
            ┌────────────────────────────────────┐
            │          User scripts              │   Lua / C++
            │   (game logic, UI, save system)    │
            └────────────────────────────────────┘
                            │  signals + getters
                            ▼
            ┌────────────────────────────────────┐
            │       DialogueBoxWidget            │   UI rendering
            │   (renders speaker / portrait /    │
            │    body / choices / continue)      │
            └────────────────────────────────────┘
                            │  signals
                            ▼
            ┌────────────────────────────────────┐
            │       DialogueRunner3D             │   Scene-graph node
            │   (Node3D + signals + state cache) │
            └────────────────────────────────────┘
                            │  callbacks
                            ▼
            ┌────────────────────────────────────┐
            │       DialogueRunner               │   Pure runtime,
            │   (mRunning, mCurrentNodeId,       │   no Node inheritance
            │    Choose / Continue / Advance)    │
            └────────────────────────────────────┘
                            │  reads
                            ▼
            ┌────────────────────────────────────┐
            │       DialogueAsset                │   Asset (cooked .oct)
            │   (DialogueGraphData — POD types)  │
            └────────────────────────────────────┘
                            ▲
                            │  parses on import
                            │
            ┌────────────────────────────────────┐
            │     DialogueJson (importer)        │   Editor-side
            │   (.dialogue → DialogueGraphData)  │
            └────────────────────────────────────┘
```

Each layer is replaceable without touching the layer below. The runner
doesn't know about scene nodes; the asset doesn't know about the runner;
the widget could be replaced with a Lua-driven custom UI.

## Why three runner layers?

`DialogueRunner` (pure), `DialogueRunner3D` (scene-graph), and the addon
singletons (`DialogueManager` / `DialogueEventDispatcher`):

- **`DialogueRunner` is intentionally `Node`-free.** It can be embedded
  in the editor preview window, a unit test, a server-side sim — any
  context that has a `DialogueAsset` and wants to walk it. It uses
  `std::function` callbacks so the scene-node layer can translate them
  into engine signals.

- **`DialogueRunner3D` adapts the runner into the scene graph.** It
  owns a `DialogueRunner` instance, a per-runner variable store, and
  the `AssetRef` to the bound asset. It re-emits the runner's callbacks
  as Polyphase signals (`OnLineChanged`, `OnChoicesChanged`, ...).

- **Addon singletons** track live runners (for hot-reload cleanup) and
  hold the global state (cross-conversation variables + global event
  dispatcher).

## POD-only types

`Source/Assets/DialogueTypes.hxx` defines every dialogue data type as a
plain struct: no virtuals, no inheritance, no exceptions. This is
deliberate — it's the layer that ships to consoles. The runner walks
these structs; the importer fills them in.

```cpp
struct DialogueNodeData
{
    std::string mId;
    DialogueNodeType mType;
    // ... only POD-friendly members ...
    std::vector<DialogueChoiceData> mChoices;
    DialogueVariableOpData mVariableOp;
};
```

The schema is duplicated in the Ink addon's `Source/Import/DialogueSchema.hxx`
under a `DialogueInkAddon::Schema::` namespace — the Ink converter operates
on the local copy and emits matching JSON, so neither addon links the
other's binary at runtime.

## Hot-reload pattern

The addon mirrors the VideoPlayer addon's hot-reload model. Three rules:

1. **`OnLoad` caches the engine API and force-links types.**
   `FORCE_LINK_CALL(DialogueAsset)`, `FORCE_LINK_CALL(DialogueRunner3D)`,
   `FORCE_LINK_CALL(DialogueBoxWidget)` ensure the static initializers
   that register these types with the engine factories actually run.

2. **Singletons own all addon-side state, never raw globals.**
   `DialogueManager`, `DialogueEventDispatcher` — anything that holds
   `AssetRef` or `ScriptFunc` (Lua-ref-backed) goes in a singleton with a
   `Clear()` method.

3. **`OnUnload` clears every singleton before nulling the API pointer.**
   The engine's hot-reload sequence is: `OnUnload` → `RemoveAllHooks` →
   asset-instance purge → factory-pointer strip → `FreeLibrary`. The
   `Clear()` calls in `OnUnload` release `AssetRef` / `ScriptFunc`
   destructors before any of that — they need to run while the addon's
   vtables are still in memory.

```cpp
static void OnUnload()
{
    DialogueAddon::DialogueManager::Get().StopAll();
    DialogueAddon::DialogueManager::Get().Clear();
    DialogueAddon::DialogueEventDispatcher::Get().Clear();
#if EDITOR
    DialogueAddon::ResetDialoguePreview();
#endif
    sAPI = nullptr;
}
```

## Cross-DLL gotchas in the design

The addon DLL is built separately from the engine. A few patterns
shaped by that:

- **Class identification uses `Node::GetClassName()` string compare**,
  not `GetType() == X::GetStaticType()`. The `GetClassName` virtual
  goes through the engine's vtable and returns the literal class name
  string from `DECLARE_FACTORY` — reliable across modules. TypeId
  equality can produce false negatives.

- **All node-typed properties use `DatumType::Node`**, not
  `DatumType::Widget` / `Quad` / etc. The engine's property-storage
  resize switch in `Property.cpp:485` only has a `Node` case — other
  node-typed datums fall through and assert during scene-load.

- **The Ink addon doesn't link to the core addon.** It duplicates the
  graph schema under a local namespace and emits matching `.dialogue`
  JSON for the core addon's importer to ingest. Two-step on purpose —
  cross-addon C++ linkage breaks hot-reload and would collide on
  shipped builds where addons merge into one binary.

## Plugin entry point — dual symbols

```cpp
#if EDITOR
extern "C" OCTAVE_PLUGIN_API int PolyphasePlugin_GetDesc(PolyphasePluginDesc* d)
{ return FillDesc(d); }
#else
extern "C" int PolyphasePlugin_GetDesc_com_polyphase_system_character_dialogue_core(
    PolyphasePluginDesc* d) { return FillDesc(d); }
#endif
```

The editor build exports a single fixed symbol the `NativeAddonManager`
looks up via `GetProcAddress`. The shipped build uses a unique
per-addon symbol so multiple addons can statically link into the same
exe without colliding. The editor-emitted `Generated/AddonPlugins.cpp`
references the unique name directly.

## The widget as a thin adapter

`DialogueBoxWidget` deliberately does not own visuals. It only:

- Looks up its child Text widgets by name.
- Holds an array of user-wired choice slot widgets.
- Holds single-node references to a Portrait Quad and a Continue Indicator.
- Connects to runner signals and toggles visibility / sets text on its
  references.

No spawning, no cloning, no asset management of its own. This makes the
widget predictable, console-light, and easy for a Lua script to
substitute with a custom UI if the user wants something the native
widget can't do.

## Where validation lives

`DialogueValidator` is editor-only and operates on the in-memory
`DialogueGraphData`. It's a pure utility — no engine-side state, no
external dependencies. Both the asset inspector and the preview window
share it.

The runner doesn't validate. If you give it a bad asset, you get
`OnDialogueError` signals at the appropriate failure point but not
upfront — that's by design (validators belong with editors, not
runtimes).

## What's NOT here

A few things from the spec that didn't make MVP:

- **Visual graph editor.** The asset inspector + interactive preview
  cover the runtime concerns; node-graph layout authoring isn't here.
- **NodeGraph dialogue nodes.** Visual scripting integration is deferred.
- **Voice-line playback.** The schema reserves a `voice` field on
  speakers and lines; the runtime doesn't currently fire `PlaySound2D`
  on line change.
- **Localization runtime.** Lines carry a `locKey` field; resolving
  that to the active locale's text is left to the user's localization
  system.
- **Ink runtime bridge.** The companion addon is import-only; no
  desktop-only `inkcpp` integration yet.

## Ship order

The addon is structured so each layer can ship independently:

1. **Asset + types + JSON parser** — could be linked into a tool that
   doesn't load the engine at all.
2. **Pure runner** — runs anywhere, no Polyphase dependency beyond
   `Stream` for save/load.
3. **Node3D wrapper + Lua** — Polyphase-specific.
4. **Widget** — Polyphase-specific, optional.
5. **Editor tooling** — `#if EDITOR`-guarded, never in the shipped
   console binary.

If a future game needs the dialogue runtime in a non-Polyphase context,
layers 1-2 are the relevant code.
