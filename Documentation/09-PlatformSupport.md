# Platform Support

Designed to ship on every Polyphase target. The runtime is exception-free
and RTTI-free; editor tooling is `#if EDITOR`-guarded so it never compiles
into a console build.

## Targets

| Platform | Status | Notes |
|---|---|---|
| Windows (editor + game) | ✅ Tested | The development target. |
| Linux (editor + game) | ✅ Should work | Same code paths as Windows. Untested in MVP. |
| 3DS | 🔶 By construction | Compiles with `-fno-rtti -fno-exceptions`. Not yet built/run on hardware. |
| Wii / GameCube | 🔶 By construction | devkitPPC. Not yet built/run on hardware. |
| Android | 🔶 By construction | Same as Linux. Not yet tested. |

## Console safety

The runtime side of this addon is written under these constraints:

- No `throw`, no `try/catch`, no exceptions of any kind.
- No `dynamic_cast`. Type discrimination uses `Node::GetClassName()`
  string compare (cross-DLL safe) or `GetType() == X::GetStaticType()`
  TypeId equality (use within the same module only).
- No `<typeinfo>`, no `<exception>`.
- `std::string`, `std::vector`, `std::unordered_map` are OK — the engine
  itself uses these on consoles.
- Exception-free JSON parser ships in-tree (no rapidjson dependency).
  Parse failures return `false` + a string error.
- All class registrations go through `DECLARE_NODE` / `DECLARE_ASSET` —
  the engine's factory machinery is already console-safe.

## Editor-only code

Anything under `Source/Editor/` is wrapped in `#if EDITOR` guards. This
includes:

- `DialogueAssetViewer` — the inspector extension.
- `DialoguePreviewWindow` — the interactive preview.
- `DialogueValidator` — the graph validator.
- The `RegisterImportExtension` call in `OnLoad` for `.dialogue` files.

In a shipped console build, these symbols don't exist. The `RegisterEditorUI`
plugin descriptor function is `nullptr`-set in non-editor builds.

## Companion Ink addon

`com.polyphase.system.character.dialogue.ink` is **importer-only** —
it's editor-only by design. The cooked `.dialogue` file produced at edit
time is what consoles consume via the core addon's runtime. No Ink
runtime code ships.

If you later want a desktop-only Ink runtime bridge, the spec calls for
gating it under `#if PLATFORM_WINDOWS || PLATFORM_LINUX`. Not currently
implemented.

## Cross-DLL gotchas

The addon compiles into its own DLL on Windows / shared object on Linux.
A few patterns that don't survive the addon/engine module boundary:

### TypeId equality

`Asset::GetType() == X::GetStaticType()` works for types defined in your
addon (because both sides resolve to the same in-DLL static factory), but
can produce false negatives for engine types like `Button`, `Text`,
`Quad`. Prefer `Node::GetClassName()` string compare for those:

```cpp
// works cross-DLL
if (n->GetClassName() && std::strcmp(n->GetClassName(), "Button") == 0) { ... }

// works only within-module
if (n->GetType() == Button::GetStaticType()) { ... }   // ⚠️ unreliable
```

### Property `DatumType::Widget`

The engine's property storage path only handles `DatumType::Node` for
node-typed property vectors and singletons (see `Property.cpp:485` —
no `Widget` / `Node3D` / `Quad` / `Text` cases). Use `DatumType::Node`
for any Node-derived property; the inspector still accepts node drops,
it just doesn't pre-filter to a specific subclass.

```cpp
// safe
outProps.push_back(Property(DatumType::Node, "Portrait Quad", this, &mPortraitQuad));

// asserts in WeakPtr::Clear during scene-load
// outProps.push_back(Property(DatumType::Widget, "Portrait Quad", this, &mPortraitQuad));
```

### Header file extensions in the addon

Header files in this addon are named `.hxx` rather than `.h`. The engine's
`NativeAddonManager` build script collects `.h`/`.hpp` source files
for compilation; addons whose paths contain `.c` substrings (like the
`com.polyphase.system.character.dialogue.core` ID — the `.c` in
`.character.` and `.core`) trigger a substring-match bug that passes
headers to the linker, hitting LNK1107.

The naming convention sidesteps it. If you build your own addon and the
ID doesn't contain `.c`, you can use `.h` normally. The engine bug fix
is a one-line edit in `NativeAddonManager.cpp:1588` (substring →
extension match).

## Save-state versioning

`Stream`-based serialization is endian-handled by the engine. The
addon's `DialogueAsset` writes a versioned header + magic-anchored
trailer (`0xD1A107C0`) for forward-compatible field additions. Older
asset files load cleanly on newer addon builds.

Per-runner save state (`SaveDialogueState`) has no internal version —
wrap it in your own version byte if you anticipate the per-runner layout
changing.

## Memory footprint

Rough order-of-magnitude on a 50-node, 5-speaker, 10-variable graph:

- `DialogueGraphData` in memory: ~10–15 KB (mostly strings).
- Per-runner overhead: ~1 KB (variable store + cached current-line strings).
- Widget-driven UI nodes are all the user's own.

Audio (voice lines) is the heavy thing; the runtime doesn't load voice
asset bytes until/unless the user explicitly plays them.

## Thread safety

The addon-owned singletons (`DialogueManager`, `DialogueEventDispatcher`)
hold mutexes around their containers — they're safe to access from any
thread. The runner itself is main-thread only; don't call `Start` /
`ContinueDialogue` / etc. from a worker.

## Hot-reload

The addon is hot-reload-safe. `OnUnload` clears every singleton state
holder before the DLL is freed:

1. `DialogueManager::StopAll()` — stops live runners, releases their
   transient state.
2. `DialogueManager::Clear()` — empties the global variable store + the
   tracking set.
3. `DialogueEventDispatcher::Clear()` — releases Lua `ScriptFunc`
   references registered via `Dialogue.OnEvent`.
4. `ResetDialoguePreview()` — clears the preview window's transient
   asset pointer.

After reload, runners re-register in `Node::Create()`. Lua scripts that
called `Dialogue.OnEvent` need to re-subscribe — their stored listener
ids are invalid across the reload.
