# DialogueBoxWidget

A `Widget` that auto-binds to a `DialogueRunner3D` and renders the current
line, the speaker label, the resolved portrait, the available choices, and
a press-to-continue indicator. No spawning or cloning at runtime — every
visual is a child node the user wires up in the editor.

## Scene shape

```
DialogueBox            (DialogueBoxWidget)
├── Speaker            (Text)              ← name from "Speaker Child" property (default: "Speaker")
├── Body               (Text)              ← name from "Body Child" property
├── Portrait           (Quad)              ← drag into "Portrait Quad" property
├── ContinueArrow      (Quad)              ← drag into "Continue Indicator" property
├── Choice0            (Button)            ← drag into "Choices" array, slot 0
├── Choice1            (Button)            ← drag into "Choices" array, slot 1
├── Choice2            (Button)            ← drag into "Choices" array, slot 2
└── Choice3            (Button)            ← drag into "Choices" array, slot 3
```

`Speaker` and `Body` are looked up by name. `Portrait` / `ContinueArrow`
and the `Choices` array are wired by reference.

You can name the child Text nodes whatever you want — change the
"Speaker Child" / "Body Child" properties to match.

## Properties

### Asset binding

| Property | Type | Default | Notes |
|---|---|---|---|
| `Runner Path` | string | `"Runner"` | Node name to find via `World::FindNode`. Falls back to a descendant search and an ancestor walk. |

### Children-by-name

| Property | Type | Default | Notes |
|---|---|---|---|
| `Speaker Child` | string | `"Speaker"` | Name of the Text widget for the speaker label. |
| `Body Child` | string | `"Body"` | Name of the Text widget for the line text. |
| `Choices Text Child` | string | `"Choices"` | Name of the Text widget for the legacy numbered-list choice mode. Used only if the Choices array is empty. |

### Choice slots (array)

| Property | Type | Notes |
|---|---|---|
| `Choices` | `Node[]` | One entry per choice slot. The widget shows the first N entries (where N = `runner:GetNumChoices()`) and hides the rest. |

Each entry is normally a `Button`. Non-Button entries get their first
`Text` descendant labelled (preferring one named `"Label"`), and you wire
your own click handler in script.

### Single-node references

| Property | Type | Notes |
|---|---|---|
| `Portrait Quad` | `Node` | Quad widget for the speaker portrait. Texture is set via `Quad::SetTexture` on every line change; hidden when no portrait is resolved. |
| `Continue Indicator` | `Node` | Any widget. Toggled on/off via `SetVisible` to flash when waiting on advance input. |

### Behaviour

| Property | Type | Default | Notes |
|---|---|---|---|
| `Typewriter Speed` | float | `40.0` | chars/sec. Set to 0 for instant text. |
| `Advance Key` | int | `32` | Engine key code; default is space. Used by the keyboard fallback in Tick. Console gamepad-A is handled by Button.Activate, not by this property. |
| `Continue Indicator Period` | float | `0.6` | Full on→off cycle in seconds. Halved internally for the per-toggle period. |

## Behaviour summary

| State | Speaker / Body | Portrait Quad | Choice slots | Continue Indicator |
|---|---|---|---|---|
| Dialogue not running | empty | hidden | hidden | hidden |
| Typewriter revealing | filling in | shown | hidden | hidden |
| Line revealed, choices shown | full text | shown | first N visible, rest hidden | hidden (player picks a choice) |
| Line revealed, no choices | full text | shown | hidden | **flashing** |
| Dialogue ended | empty | hidden | hidden | hidden |

## Choice activation

When a `Choice` slot is a `Button`, the widget connects its `"Activated"`
signal at `Start()`. On engine activation (mouse click, keyboard, gamepad
A on a focused button), the widget matches the button to its array index
and calls `runner:ChooseDialogueOption(i)`.

Gamepad nav is wired automatically: `SetNavUp` / `SetNavDown` between
adjacent visible Button slots, and `Button::SetSelectedButton` is called
on the first visible slot so gamepad has immediate focus.

### Authoring tip — number of slots

Wire one slot per max-choice node in your dialogue. The demo's
`Sample.dialogue` has `intro_choice` with 4 choices; everything else has
2-3. Wiring 4 slots covers it. Wiring fewer logs a warning per
over-budget rebuild but doesn't crash — choices beyond the wired count
just don't appear.

### Hiding correctly

The widget proactively clears `Button::sSelectedButton` and clears nav
links on every rebuild, so a previously-visible-now-hidden Button can't
be activated by a stray A press or d-pad navigation. This is important
when a node with N choices advances to a node with M < N (or 0) choices.

## Speaker name resolution

The widget reads `runner:GetCurrentSpeakerName()` for the Speaker label —
the runner resolves this from the asset's speaker table. If the speaker
id isn't declared, the raw id is used as a fallback.

Per-line `speaker` overrides the speaker table lookup; the runner uses
whichever speaker id the current line specifies.

## Continue indicator

The flash logic:

- Active iff `runner:IsDialogueRunning()` AND typewriter done AND
  `runner:GetNumChoices() == 0`.
- When entering the active state, the indicator shows immediately (no
  half-period delay).
- Toggles every `Continue Indicator Period / 2` seconds.
- Hidden as soon as conditions stop being met (e.g. the player picks an
  advance, the next line starts revealing).

The widget never resizes or repositions the Quad — your editor anchors
stick.

## Programmatic binding

```cpp
DialogueRunner3D* runner = ...;
DialogueBoxWidget* box = ...;
box->SetRunner(runner);  // disconnects the previous, connects this one
```

```lua
-- Lua doesn't expose SetRunner directly; set the Runner Path property
-- via the inspector, or rename/move the runner node so the path matches.
```

## Console testing

The widget uses `Button::SetHandleGamepad` (engine default `true`) so
gamepad A on the focused choice activates without any extra wiring.
D-pad up/down navigates between slots. The widget handles same-frame
double-trigger correctly (engine's `sSelButtonChangedThisFrame`).

For the line-advance flow on console — where there's no keyboard for the
Advance Key — wire a script that listens for gamepad A and calls
`runner:ContinueDialogue()` when `runner:GetNumChoices() == 0`. See the
`startTalking.lua` script in the demo project for a battle-tested
template that handles same-frame double-advance via the
`OnChoiceSelected` signal.

## Lua-driven alternative

If you don't want the widget at all, drop it from the scene and listen
to the runner's signals from your own script. `OnLineChanged` /
`OnChoicesChanged` give you everything the widget consumes — you can
build a totally custom UI.
