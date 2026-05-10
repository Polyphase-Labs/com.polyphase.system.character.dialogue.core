# Troubleshooting

Issues we've actually hit while building this. The fix is sometimes
scene-side, sometimes a one-line change.

## "Failed to import Asset. Unrecognized source asset extension."

You dropped a `.dialogue.json` file. The editor's import dispatcher
(`ActionManager.cpp:4140`) splits the filename at the **last** `.` only,
so it sees `.json` and doesn't know what to do.

**Fix:** rename to `.dialogue` (single extension). The file content is
still JSON. Configure your text editor to syntax-highlight `.dialogue`
as JSON.

## "ChooseDialogueOption: index out of range"

A `Button::Activated` signal fired with an index past the runner's
current choice count. Most common cause: a stale focused button on a
hidden choice slot.

**What was happening (before the fix):** when transitioning from a
choice node (4 visible buttons, `Choice0` focused) to a no-choice line,
the widget hid all buttons but the engine's `Button::sSelectedButton`
still pointed at `Choice0`. The next A press activated the hidden button
→ `ChooseDialogueOption(0)` → runner has 0 choices → error.

**Fixed in current build.** If you're still seeing this:

- Make sure your scene's choice buttons are wired into the **Choices**
  array property in slot order (`Choice0` at index 0, etc.).
- Confirm the widget rebuild log shows the right number of slots:
  `runner has N choices but only M slots wired` means you need more
  array entries.
- Look for any other code (custom Lua scripts, stray engine bindings)
  that calls `ChooseDialogueOption` directly with a hard-coded index.

## "Cannot add a destroyed node as a child"

The engine logs this when `Node::AddChild` is called with a
freshly-allocated-but-flagged-destroyed node. This was a symptom of a
broken Clone path in an earlier widget design that has since been
removed.

**Current build:** the widget no longer clones a `TemplateChoice` —
choices are user-wired buttons in the `Choices` array. If you see this
warning in current code, it's coming from somewhere else (a custom
Clone you wrote, or a different widget).

## Assertion in `WeakPtr::Clear` during project / scene open

Backtrace shows `Datum::SetNode → WeakPtr<Node>::Clear → OCT_ASSERT`.

**Cause:** a property declared with `DatumType::Widget` (or `Node3D` /
`Quad` / `Text` / etc.) on a `NodePtrWeak` field. The engine's property
storage path (`Property.cpp:485`) only has a resize case for
`DatumType::Node`. Other node-typed datum types fall through and leave
`mData.n` uninitialized, then `SetNode` writes through a garbage
`WeakPtr`.

**Fix:** declare node-ref properties as `DatumType::Node`:

```cpp
// safe
outProps.push_back(Property(DatumType::Node, "My Widget", this, &mWidgetRef));

// crashes on scene-load
// outProps.push_back(Property(DatumType::Widget, "My Widget", this, &mWidgetRef));
```

The inspector still accepts node drops; you lose the inspector-side
filter to a specific subclass, which is purely cosmetic.

## "no Text descendant" / "no Button descendant" warnings

The widget walks the spawned/wired tree using `Node::GetClassName()`
string compare against `"Text"` and `"Button"`. The warnings fire when
no match is found.

**Likely causes:**

- The widgets are bare `Node` instances rather than `Button` / `Text`
  widgets. In the editor, use **Add Node → Widgets → Button** /
  **Text** specifically, not generic Node.
- The class name is something else for some reason. The widget will
  log a one-shot tree dump via `LogDebug` showing the actual classes
  found in the spawned subtree — read that to diagnose.

## "no DialogueRunner3D found via Runner Path"

The widget's runner lookup tries (in order):

1. `World::FindNode(mRunnerPath)` — world-wide name match.
2. `widget->FindChild(mRunnerPath, true)` — descendant search.
3. Ancestor walk looking for any `DialogueRunner3D` among each parent's
   direct children.

**Fix:** make sure a `DialogueRunner3D` node in your scene has the same
name as the widget's `Runner Path` property (default: `"Runner"`). Or
change the property to match your runner's name.

## Portrait Quad shows nothing / wrong texture

Check the editor log for one of:

- `portrait asset 'X' not found` — the asset name on the line / speaker
  doesn't match any texture in your project. Names are case-sensitive on
  some platforms.
- `portrait asset 'X' is a 'Y', expected 'Texture'` — the asset name
  resolves to something other than a Texture asset.
- `Portrait Quad is wired to a 'Y', expected 'Quad'` — the property is
  bound to a non-Quad widget.

The widget hides the Quad on any of these so you don't render a stale
texture from the previous line.

## "no choice rendering set up"

Neither the `Choices` array nor the `Choices Text Child` Text widget
were found at `Start()`.

**Fix:** wire one of them.

- Per-button mode (preferred): drag your choice Button widgets into the
  `Choices` array property.
- Fallback text mode: add a Text widget under the `DialogueBox` named
  `Choices` (or whatever you set `Choices Text Child` to). The widget
  will render `1. Foo\n2. Bar` and read keyboard 1-9 in `Tick`.

## Continue indicator doesn't flash

Check:

- Did you wire the `Continue Indicator` property to a widget? (Optional
  — if unwired, the indicator system is silently inactive.)
- Is the typewriter still running? The indicator is hidden during
  reveal.
- Are there choices on the current line? The indicator stays hidden
  when the player should be picking, not advancing.
- Is `Continue Indicator Period` ≤ 0? Try the default 0.6.

## Dialogue auto-runs even with `Auto Start = false`

Something is calling `runner:StartDialogue()` automatically. Most
common: the demo `dialogue_demo.lua` in earlier versions called
`StartDialogue` from its `Start()`. The current version doesn't, but
your project might still have a copy of the old one.

**Fix:** `grep -rn "StartDialogue" Scripts/` and find the offender.

## Hot-reload crashes the editor

If the editor crashes inside `DrawWindows` (or any UI redraw) after
hot-reloading the addon, it's usually because the addon retained a
callback / Lua ref / asset ref past `OnUnload`.

**The addon's own `OnUnload` does the right thing** — clears the
`DialogueManager` runner set, the global variable store, the
`DialogueEventDispatcher`, the preview window state. If you've added
your own singletons to the addon, audit them too.

User-written Lua that subscribed to `Dialogue.OnEvent` doesn't crash on
reload — the dispatcher's `Clear()` releases the stored `ScriptFunc`
refs cleanly. You'll just need to re-subscribe after reload.

## Build fails: "DialogueRunner3D::LoadStream did not override"

The Node-base `LoadStream` signature is
`(Stream&, Platform, uint32_t version)` — three args. Asset's is
`(Stream&, Platform)` — two args. Easy mix-up.

```cpp
virtual void LoadStream(Stream& stream, Platform platform, uint32_t version) override;
```

Forward to the base:

```cpp
void DialogueRunner3D::LoadStream(Stream& stream, Platform platform, uint32_t version)
{
    Node3D::LoadStream(stream, platform, version);
    // ...
}
```

## Build fails: D9024 / LNK1107 inside the addon's .h files

The engine's build-script source-gathering walks the addon's `Source/`
tree for `.h` / `.hpp` / `.cpp` / `.c` files and passes them all to cl.
The list filter is `path.find(".c") != npos` — which false-matches any
addon path containing `.c` in a directory name (e.g. our addon ID has
`.character.` and `.core`).

Two workarounds (we're using both):

1. **Engine fix** in `NativeAddonManager.cpp:1588`: replace the substring
   match with extension match. One-line change.
2. **Header file extensions**: rename `.h` → `.hxx`. The engine's source
   gatherer doesn't pick up `.hxx`, so headers stay out of the cl
   command line entirely.

The current addon uses `.hxx` for all internal headers as a
belt-and-suspenders guard. Engine includes (`Stream.h`, `Plugins/...`)
stay as `.h` because they're in the engine source tree.

## Diagnostic logging I want

Several warning paths in the addon log just-enough information to
diagnose the immediate problem without flooding the console. If you
need more verbose tracing while developing:

- The widget's `RebuildChoiceButtons` logs slot/runner discrepancy.
- `ApplyPortrait` logs missing/mistyped portrait assets.
- The runner's `EmitDialogueError` is called by every fail path
  (`StartDialogue` with no asset, `StartDialogueAtNode` with bad id,
  out-of-range `ChooseDialogueOption`, hop-limit overflow, asset-side
  errors). Connect to `OnDialogueError` to surface them in your own UI.

```lua
runner:ConnectSignal("OnDialogueError", self, function(self, message)
    Log.Error("dialogue: " .. message)
end)
```
