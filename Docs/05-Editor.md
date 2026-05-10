# Editor

## Tools menu

- `Tools → Dialogue → Open Preview` — opens the interactive preview window.

## DialogueAsset inspector

When a DialogueAsset is selected, the inspector shows:

- The standard property grid: Start Node, Node Count, Speaker Count, Variable Count.
- An `Open Dialogue Preview` button.
- A `Run Validation` button — logs warnings / errors to the console.
- An expandable `Validation` panel listing issues inline.

## Dialogue Preview window

Type a `DialogueAsset` name and click `Load`. Then `Start` runs the
conversation in a transient runner; the window shows:

- Current speaker + body text.
- Numbered choice buttons.
- A `Continue` button when no choices are pending.
- An expandable variable inspector and event log.

The preview's variable store is independent of any in-scene runner — useful
for stepping through a graph without disturbing live game state.

## Validation rules

- Missing start node, broken jump targets, broken choice targets.
- Duplicate node ids.
- Speakers / variables referenced but not declared.
- Unreachable End nodes (warning).
- Unreachable nodes from start (info).

Run from the inspector's `Run Validation` button or open the inspector's
`Validation` panel.
