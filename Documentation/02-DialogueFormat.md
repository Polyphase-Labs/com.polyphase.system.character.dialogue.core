# Dialogue Format

`.dialogue` is the source authoring format. The editor importer reads it and
produces a `DialogueAsset` — a Polyphase asset that ships in cooked builds.

The file is JSON. Comments (`//` and `/* */`) are tolerated for
hand-authoring; they're stripped at parse time and not preserved on save.

## Top-level shape

```json
{
    "start":     "<node id>",
    "speakers":  [ ... ],
    "variables": [ ... ],
    "nodes":     [ ... ],
    "links":     [ ... ]    // optional, used for graph-layout metadata only
}
```

| Field | Required | Notes |
|---|---|---|
| `start` | yes | Node id where the conversation begins. Importer falls back to the first node + a warning if missing. |
| `speakers` | optional | Speaker definitions used for display name + default portrait resolution. |
| `variables` | optional | Variable declarations + defaults. |
| `nodes` | yes | The graph itself. |
| `links` | optional | Editor metadata only — runtime flow uses `choice.target` and `node.jumpTargetId`. |

## Speakers

```json
"speakers": [
    {
        "id":       "guard",          // referenced from node.speaker
        "name":     "Captain Pasukan",// shown in the box's Speaker label
        "portrait": "Guard_Idle",     // default texture asset name (per-line override beats this)
        "voice":    "GuardVoice"      // reserved for voice-line playback (not yet wired in MVP)
    }
]
```

If a node's `speaker` doesn't match any declared speaker, the runner uses the
raw id as the display name. The validator flags this as a warning.

## Variables

```json
"variables": [
    { "name": "gold",        "type": "int",    "default": 100 },
    { "name": "met_guard",   "type": "bool",   "default": false },
    { "name": "affinity",    "type": "float",  "default": 0.0 },
    { "name": "current_quest","type": "string","default": "" }
]
```

Types: `bool` / `int` / `float` / `string`. Defaults are seeded into the
variable store when `StartDialogue()` is first called.

## Nodes

The list of nodes is order-independent — control flow happens via
`jumpTargetId` and `choice.target`.

### Common fields

```json
{
    "id":   "<unique string>",
    "type": "line | choice | branch | event | set_variable | jump | end"
}
```

### Per-type fields

#### `line`

The most common type — a speaker says something, optionally offers choices.

```json
{
    "id": "intro", "type": "line", "speaker": "guard",
    "portrait":     "Guard_Happy",     // optional per-line override
    "text":         "Welcome, traveler.",
    "locKey":       "guard.intro",     // optional localization key
    "voice":        "GuardWelcome01",  // optional voice line asset name
    "tags":         ["loc:guard.intro"],
    "jumpTargetId": "next_id",         // advance target if no choices
    "variableOp":   { ... },           // optional side effect on entry
    "conditions":   [ ... ],           // optional gate (filters this node out of choice lists)
    "choices":      [ ... ]            // optional list of player choices
}
```

#### `choice` element

```json
{
    "id":         "polite",
    "text":       "Just passing through.",
    "locKey":     "guard.polite",
    "target":     "polite_resp",
    "tags":       ["polite"],
    "conditions": [
        { "var": "gold", "op": "ge", "value": 50 }
    ]
}
```

`conditions` filter the choice from `GetAvailableChoices()` — if the player
doesn't have the gold, the option doesn't appear.

#### `event`

Fires `OnDialogueEvent(eventName)` then advances to `jumpTargetId`. Use
these as named "scenes ended this way" markers your scripts react to.

```json
{ "id": "rude_ending", "type": "event", "eventName": "GuardAggro",
  "jumpTargetId": "rude_ending_line" }
```

#### `set_variable`

Side-effect node with no UI. Applies the `variableOp` then advances.

```json
{ "id": "gain_rep", "type": "set_variable",
  "variableOp": { "op": "add", "var": "rep", "value": 1 },
  "jumpTargetId": "next" }
```

#### `branch`

Walks `choices` (no UI) and takes the first one whose conditions pass.
Useful for state-driven branching.

```json
{ "id": "rep_check", "type": "branch",
  "jumpTargetId": "neutral",     // fallback if no choice's conditions pass
  "choices": [
    { "id": "high",   "target": "praise",
      "conditions": [{ "var": "rep", "op": "ge", "value": 10 }] },
    { "id": "shamed", "target": "scorn",
      "conditions": [{ "var": "rep", "op": "less", "value": 0 }] }
  ]
}
```

#### `jump`

Pure unconditional jump. Equivalent to a `line` with no text but cleaner
intent.

```json
{ "id": "rejoin", "type": "jump", "jumpTargetId": "common_path" }
```

#### `end`

Terminates the conversation. The runner stops, `IsDialogueRunning()`
returns `false`, and `OnDialogueFinished` fires.

```json
{ "id": "end", "type": "end" }
```

## Conditions

```json
{ "var": "<variable name>", "op": "<op>", "value": <typed-value> }
```

Operators (case-sensitive strings):

| Op | Meaning |
|---|---|
| `exists` | Variable has been set (any type) |
| `not_exists` | Variable hasn't been set |
| `equals` / `eq` | Equal |
| `not_equals` / `ne` | Not equal |
| `greater` / `gt` | Strictly greater |
| `ge` | Greater or equal |
| `less` / `lt` | Strictly less |
| `le` | Less or equal |

Numeric comparisons promote `bool → int → float`. String comparisons are
lexicographic. Comparing across number/string boundaries returns `false`
(not an error). A condition referencing a variable that hasn't been set
returns `false` for everything except `exists`/`not_exists`.

## Variable operations

```json
"variableOp": { "op": "<op>", "var": "<name>", "value": <typed-value> }
```

| Op | Meaning |
|---|---|
| `set` | Replace the variable's value |
| `add` | Add to int or float |
| `sub` | Subtract |
| `toggle` | Boolean negate |
| `none` | (default — no-op) |

Set operations preserve the variable's declared type. If you `set` a
declared `int` variable to a `string` literal in JSON, the importer keeps
the int slot but resets it to zero — declared type wins.

## Tags

`tags` is a free-form `string[]` per node and per choice. The runtime
doesn't interpret them; your scripts can read them via the underlying
`DialogueNodeData` if you want custom routing.

The Ink importer also uses tags as routing hints during conversion (e.g.
`#speaker:guard`, `#portrait:Guard_Happy`, `#event:OpenDoor`) — those map
to the appropriate fields on the produced node, but in the converted
`.dialogue` file they appear as their resolved field, not as raw tags.

## A complete example

The demo project ships `Assets/Dialogue/Sample.dialogue` — a castle-gate
scenario with four end events and per-line portrait expressions. Read it
alongside this doc.

## Binary serialization

`DialogueAsset::SaveStream` writes a versioned header (`DIALOGUE_ASSET_VERSION = 1`)
followed by speakers, variables, nodes, links, and a magic-anchored trailer
(`0xD1A107C0`) reserved for forward-compatible fields. Older `.oct` files
load cleanly on newer addon builds; new fields appear after the trailer
magic and are presence-checked individually.
