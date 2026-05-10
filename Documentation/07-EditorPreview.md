# Editor Preview

The addon ships an interactive preview window so you can step through a
`DialogueAsset` without dropping a `DialogueRunner3D` into a scene.
Useful for iterating on writing, validating choice flow, and verifying
condition gating.

## Opening the preview

**Tools → Dialogue → Open Preview** — or press `Open Dialogue Preview`
on a `DialogueAsset`'s inspector panel.

## Window layout

```
┌─ Dialogue Preview ─────────────────────────────┐
│ Asset name: [_________________________] [Load] │
│                                                │
│ Loaded: Sample                                 │
│ Start: intro    Nodes: 17                      │
│                                                │
│ ─────────────────────                          │
│ [Start]  [Reset]                               │
│                                                │
│ Captain Pasukan                                │
│ Halt! These are the gates of Kerajaan Selapang.│
│                                                │
│ 1. Just passing through.                       │
│ 2. None of yours.                              │
│                                                │
│ ▼ Variables                                    │
│   • gold = 120 (int)                           │
│   • met_guard = false (bool)                   │
│                                                │
│ ▼ Event Log                                    │
│   -- started --                                │
│   loaded: Sample                               │
└────────────────────────────────────────────────┘
```

## Usage

1. **Type the asset name** (no extension — just the name as it appears
   in the asset browser, e.g. `Sample`).
2. Click **Load**. The asset is fetched via `api->LoadAsset(name)` and
   bound to the preview's transient runner.
3. Click **Start** to begin the conversation. Speaker, body text, and
   available choices appear.
4. Click a choice (or **Continue** if no choices) to advance.
5. **Reset** clears the runner and variable store.
6. The **Variables** panel shows the current variable store live.
7. The **Event Log** shows started / finished / event firings.

## Isolation

The preview owns its own `DialogueRunner` and `DialogueVariableStore` —
it does **not** affect any in-scene `DialogueRunner3D`'s state. Global
variables set via `Dialogue.SetGlobalBool` from script during PIE are
also separate from the preview store.

You can use the preview alongside a running scene without interference.

## Asset inspector extension

Selecting a `DialogueAsset` in the asset browser shows the standard
property grid plus an addon-extended panel:

```
Dialogue Summary
  • Start: intro
  • Nodes: 17
  • Speakers: 2
  • Variables: 2

[ Open Dialogue Preview ]   [ Run Validation ]

▼ Validation
  ✓ No issues.
```

`Run Validation` writes warnings to the engine console; the inline panel
under `Validation` shows the same list.

## Validator

The validator analyses the graph and reports:

- Missing or unknown `start` node
- Duplicate node ids
- Choice / jump targets that don't resolve
- Speaker ids referenced but not declared in `speakers`
- Condition / variable-op variables not declared in `variables` (info)
- Unreachable End nodes (warning — likely an infinite loop)
- Nodes unreachable from `start` (info — orphaned content)

Severity levels:

| Color | Severity | Meaning |
|---|---|---|
| red | Error | Will likely crash or behave unpredictably at runtime |
| yellow | Warning | Probably wrong, should fix |
| grey | Info | Might be intentional, worth noting |

The validator runs on the in-memory asset; it doesn't re-read the source
`.dialogue` file. Re-import the file if you've edited the source.

## Workflow

Typical authoring loop:

1. Edit `.dialogue` source in your text editor.
2. Drag the `.dialogue` file into the asset browser to re-import.
3. Open the asset; click `Run Validation`.
4. Fix any errors.
5. Click `Open Dialogue Preview`.
6. Walk through every branch you care about.
7. Adjust source, re-import, re-preview.

## Limitations

- The preview's transient runner has no `DialogueRunner3D` parent, so
  signals fired during preview don't reach scripts that listen on real
  in-scene runners.
- The preview doesn't render portraits — it only shows the resolved
  portrait name in the variable inspector if you're looking. To verify
  portrait textures load correctly, drop the runner into a real scene.
- The preview reads the in-memory cooked asset, so source-level comments
  and exact JSON formatting aren't visible.

## Hot-reload

`Tools → Addons → Reload Native Addons` clears the preview state safely
— the addon's `OnUnload` calls `ResetDialoguePreview()` so the cached
asset pointer doesn't outlive the DLL.
