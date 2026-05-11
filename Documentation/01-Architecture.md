# com.polyphase.system.character.dialogue.core Architecture

## Purpose

A console-safe runtime dialogue system for the Polyphase engine. Ships as a

## Runtime Model

- Addon entrypoint initializes engine API access, registers types, and binds Lua surface.
- Runtime state is owned by addon managers and node/asset instances under `Source/`.
- Cross-addon communication is expected through Lua APIs and bridge variables, not direct sibling includes.

## Main Components

- `Source/` contains C++ runtime implementation.
- `Source/Lua/` (when present) exposes script APIs.
- `Source/Nodes/` and `Source/Assets/` (when present) define scene/editor-facing types.
- `Source/Runtime/` (when present) holds singleton managers and state logic.

## Data and Persistence

- Stream serialization follows addon-local `SaveStream`/`LoadStream` patterns.
- Global save integration flows through `RpgSave` where applicable.
- Bridge variable publishing is used for dialogue/quest/UI observability.

## Extension Points

- Lua bindings for gameplay scripts.
- Dialogue event/variable integration points.
- Node and asset properties exposed in editor inspectors.
