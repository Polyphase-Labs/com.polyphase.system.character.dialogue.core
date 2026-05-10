#pragma once

#include "EngineTypes.h"
#include "Log.h"
#include "Engine.h"

#include "Nodes/DialogueRunner3D.hxx"

#include "LuaBindings/Node_Lua.h"
#include "LuaBindings/LuaUtils.h"

#if LUA_ENABLED

// Metatable name MUST match DECLARE_NODE(DialogueRunner3D, Node3D) so that
// Script::CallFunction can resolve it via luaL_getmetatable(L, node->GetClassName()).
#define DIALOGUE_RUNNER_3D_LUA_NAME "DialogueRunner3D"
#define DIALOGUE_RUNNER_3D_LUA_FLAG "cfDialogueRunner3D"
#define CHECK_DIALOGUE_RUNNER_3D(L, arg) \
    static_cast<DialogueRunner3D*>(CheckNodeLuaType(L, arg, DIALOGUE_RUNNER_3D_LUA_NAME, DIALOGUE_RUNNER_3D_LUA_FLAG));

struct DialogueRunner3D_Lua
{
    // Asset binding
    static int SetDialogueAsset(lua_State* L);
    static int GetDialogueAsset(lua_State* L);

    // Lifecycle
    static int StartDialogue(lua_State* L);
    static int StartDialogueAtNode(lua_State* L);
    static int StopDialogue(lua_State* L);
    static int IsDialogueRunning(lua_State* L);

    // Flow
    static int ContinueDialogue(lua_State* L);
    static int ChooseDialogueOption(lua_State* L);

    // Queries
    static int GetCurrentSpeaker(lua_State* L);
    static int GetCurrentSpeakerName(lua_State* L);
    static int GetCurrentText(lua_State* L);
    static int GetCurrentLocKey(lua_State* L);
    static int GetCurrentPortrait(lua_State* L);
    static int GetNumChoices(lua_State* L);
    static int GetChoiceText(lua_State* L);
    static int GetChoiceId(lua_State* L);

    // Variables
    static int SetBool(lua_State* L);
    static int GetBool(lua_State* L);
    static int SetInt(lua_State* L);
    static int GetInt(lua_State* L);
    static int SetFloat(lua_State* L);
    static int GetFloat(lua_State* L);
    static int SetString(lua_State* L);
    static int GetString(lua_State* L);

    static void Bind();
};

#endif
