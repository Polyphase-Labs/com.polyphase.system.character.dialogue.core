#pragma once

#include "EngineTypes.h"
#include "Engine.h"
#include "LuaBindings/LuaUtils.h"

#if LUA_ENABLED

// Global Dialogue.* Lua module — exposes addon-wide variable storage and the
// global event-dispatcher. Per-conversation operations live on
// DialogueRunner3D's metatable (see DialogueRunner3D_Lua.h).
struct Dialogue_Lua
{
    // Variable I/O against DialogueManager::GetGlobalStore().
    static int SetGlobalBool(lua_State* L);
    static int GetGlobalBool(lua_State* L);
    static int SetGlobalInt(lua_State* L);
    static int GetGlobalInt(lua_State* L);
    static int SetGlobalFloat(lua_State* L);
    static int GetGlobalFloat(lua_State* L);
    static int SetGlobalString(lua_State* L);
    static int GetGlobalString(lua_State* L);

    // Connect / disconnect Lua listeners to dialogue events.
    // Dialogue.OnEvent("OpenDoor", function(name, asset) ... end) -> id
    // Dialogue.OffEvent(id) -> bool
    static int OnEvent(lua_State* L);
    static int OffEvent(lua_State* L);

    static void Bind();
};

#endif
