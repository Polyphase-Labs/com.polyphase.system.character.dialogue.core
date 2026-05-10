#pragma once

// Cached PolyphaseEngineAPI* accessor. Set once in the addon's OnLoad and read
// from any addon source file that needs LogDebug, GetLua, IsKeyJustPressed,
// PlaySound2D, etc. without threading the pointer through constructors.

struct PolyphaseEngineAPI;

namespace DialogueAddon
{
    PolyphaseEngineAPI* GetEngineAPI();
    void SetEngineAPI(PolyphaseEngineAPI* api);
}
