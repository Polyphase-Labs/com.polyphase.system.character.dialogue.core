# com.polyphase.system.character.dialogue.core Lua API

## API Surface

- Lua functions are implemented under `Source/Lua/` when this addon exposes script APIs.
- Global table names and exact signatures should be treated as source-of-truth from the addon Lua binding files.

## Discovery Workflow

1. Open `Source/Lua/` for this addon.
2. Find `Bind()` functions and `lua_setglobal` calls to identify table names.
3. Review function bodies for argument order, return values, and side effects.

## Integration Guidance

- Wrap calls in `pcall` from gameplay scripts when optional addons may be unloaded.
- Prefer deterministic IDs (character IDs, inventory IDs, table IDs) over scene-name coupling.
- Mirror critical state to dialogue globals for UI and condition checks.

## Common Usage Pattern

```lua
if _G["SomeGlobalTable"] ~= nil then
    -- call addon API safely
end
```

Use the actual global table name from `Source/Lua/*_Lua.cpp` (for example `Inventory`, `Quest`, `Orge`, `PointClick`).
