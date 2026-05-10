# Runtime API

## `DialogueRunner3D` (Node3D)

Lifecycle:

- `StartDialogue()`, `StartDialogueAtNode(id)`, `StopDialogue()`, `IsDialogueRunning()`.

Flow:

- `ContinueDialogue()` advances when no choices are pending.
- `ChooseDialogueOption(index)` / `ChooseDialogueOptionById(id)` selects a choice.

Queries (used by the widget and Lua scripts):

- `GetCurrentSpeaker()`, `GetCurrentText()`, `GetCurrentLocKey()`.
- `GetNumChoices()`, `GetChoiceText(i)`, `GetChoiceId(i)`.

Variables (per-runner store; falls through to global store when `Use Global Store` is true):

- `SetBool/Int/Float/String(name, value)`, `GetBool/Int/Float/String(name, default)`.

Save / load:

- `SaveDialogueState(outBytes)`, `LoadDialogueState(bytes)` for slot-style state I/O.
- `SaveStream` / `LoadStream` are wired into the engine's scene save pipeline.

## Signals

`DialogueRunner3D` emits these signals — connect via `node:ConnectSignal(name, listener, fn)`:

| Signal | Args |
|---|---|
| `OnDialogueStarted`  | (none) |
| `OnDialogueStopped`  | (none) |
| `OnDialogueFinished` | (none) |
| `OnLineChanged`      | (speakerId, text, locKey) |
| `OnChoicesChanged`   | (numChoices) |
| `OnChoiceSelected`   | (choiceId, choiceText) |
| `OnDialogueEvent`    | (eventName) |
| `OnDialogueError`    | (message) |
