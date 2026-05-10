#include "Lua/Dialogue_Lua.hxx"

#include "Runtime/DialogueManager.hxx"
#include "Runtime/DialogueEvents.hxx"

#include "ScriptFunc.h"
#include "Log.h"

#if LUA_ENABLED

namespace
{
    DialogueVariableStore& GS()
    {
        return DialogueAddon::DialogueManager::Get().GetGlobalStore();
    }
}

int Dialogue_Lua::SetGlobalBool(lua_State* L)
{
    const char* name = CHECK_STRING(L, 1);
    bool v = CHECK_BOOLEAN(L, 2);
    if (name) GS().SetBool(name, v);
    return 0;
}

int Dialogue_Lua::GetGlobalBool(lua_State* L)
{
    const char* name = CHECK_STRING(L, 1);
    bool def = false;
    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) def = lua_toboolean(L, 2) != 0;
    lua_pushboolean(L, GS().GetBool(name ? name : "", def) ? 1 : 0);
    return 1;
}

int Dialogue_Lua::SetGlobalInt(lua_State* L)
{
    const char* name = CHECK_STRING(L, 1);
    int32_t v = (int32_t)CHECK_INTEGER(L, 2);
    if (name) GS().SetInt(name, v);
    return 0;
}

int Dialogue_Lua::GetGlobalInt(lua_State* L)
{
    const char* name = CHECK_STRING(L, 1);
    int32_t def = 0;
    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) def = (int32_t)lua_tointeger(L, 2);
    lua_pushinteger(L, GS().GetInt(name ? name : "", def));
    return 1;
}

int Dialogue_Lua::SetGlobalFloat(lua_State* L)
{
    const char* name = CHECK_STRING(L, 1);
    float v = (float)CHECK_NUMBER(L, 2);
    if (name) GS().SetFloat(name, v);
    return 0;
}

int Dialogue_Lua::GetGlobalFloat(lua_State* L)
{
    const char* name = CHECK_STRING(L, 1);
    float def = 0.0f;
    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) def = (float)lua_tonumber(L, 2);
    lua_pushnumber(L, GS().GetFloat(name ? name : "", def));
    return 1;
}

int Dialogue_Lua::SetGlobalString(lua_State* L)
{
    const char* name = CHECK_STRING(L, 1);
    const char* v = CHECK_STRING(L, 2);
    if (name) GS().SetString(name, v ? v : "");
    return 0;
}

int Dialogue_Lua::GetGlobalString(lua_State* L)
{
    const char* name = CHECK_STRING(L, 1);
    const char* def = "";
    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) def = lua_tostring(L, 2);
    std::string out = GS().GetString(name ? name : "", def ? def : "");
    lua_pushstring(L, out.c_str());
    return 1;
}

int Dialogue_Lua::OnEvent(lua_State* L)
{
    const char* eventName = CHECK_STRING(L, 1);
    if (!lua_isfunction(L, 2))
    {
        LogWarning("Dialogue.OnEvent: arg 2 must be a function");
        lua_pushinteger(L, 0);
        return 1;
    }
    ScriptFunc fn(L, 2);
    auto id = DialogueAddon::DialogueEventDispatcher::Get().Connect(eventName ? eventName : "", fn);
    lua_pushinteger(L, id);
    return 1;
}

int Dialogue_Lua::OffEvent(lua_State* L)
{
    int32_t id = (int32_t)CHECK_INTEGER(L, 1);
    bool ok = DialogueAddon::DialogueEventDispatcher::Get().Disconnect(id);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

void Dialogue_Lua::Bind()
{
    lua_State* L = GetLua();
    const int startTop = lua_gettop(L);

    lua_newtable(L);
    const int t = lua_gettop(L);

    lua_pushcfunction(L, SetGlobalBool);   lua_setfield(L, t, "SetGlobalBool");
    lua_pushcfunction(L, GetGlobalBool);   lua_setfield(L, t, "GetGlobalBool");
    lua_pushcfunction(L, SetGlobalInt);    lua_setfield(L, t, "SetGlobalInt");
    lua_pushcfunction(L, GetGlobalInt);    lua_setfield(L, t, "GetGlobalInt");
    lua_pushcfunction(L, SetGlobalFloat);  lua_setfield(L, t, "SetGlobalFloat");
    lua_pushcfunction(L, GetGlobalFloat);  lua_setfield(L, t, "GetGlobalFloat");
    lua_pushcfunction(L, SetGlobalString); lua_setfield(L, t, "SetGlobalString");
    lua_pushcfunction(L, GetGlobalString); lua_setfield(L, t, "GetGlobalString");
    lua_pushcfunction(L, OnEvent);         lua_setfield(L, t, "OnEvent");
    lua_pushcfunction(L, OffEvent);        lua_setfield(L, t, "OffEvent");

    lua_setglobal(L, "Dialogue");

    OCT_ASSERT(lua_gettop(L) == startTop);
}

#endif
