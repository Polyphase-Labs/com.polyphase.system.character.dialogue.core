#include "Lua/DialogueRunner3D_Lua.hxx"

#include "LuaBindings/Node3d_Lua.h"
#include "LuaBindings/Asset_Lua.h"
#include "LuaBindings/LuaUtils.h"

#include "Assets/DialogueAsset.hxx"

#if LUA_ENABLED

int DialogueRunner3D_Lua::SetDialogueAsset(lua_State* L)
{
    DialogueRunner3D* dr = CHECK_DIALOGUE_RUNNER_3D(L, 1);
    Asset* asset = nullptr;
    if (!lua_isnil(L, 2))
    {
        asset = CHECK_ASSET(L, 2);
    }
    DialogueAsset* d = (asset != nullptr && asset->GetType() == DialogueAsset::GetStaticType())
                       ? static_cast<DialogueAsset*>(asset) : nullptr;
    dr->SetDialogueAsset(d);
    return 0;
}

int DialogueRunner3D_Lua::GetDialogueAsset(lua_State* L)
{
    DialogueRunner3D* dr = CHECK_DIALOGUE_RUNNER_3D(L, 1);
    Asset_Lua::Create(L, dr->GetDialogueAsset(), true /*allowNull*/);
    return 1;
}

int DialogueRunner3D_Lua::StartDialogue(lua_State* L)
{
    DialogueRunner3D* dr = CHECK_DIALOGUE_RUNNER_3D(L, 1);
    dr->StartDialogue();
    return 0;
}

int DialogueRunner3D_Lua::StartDialogueAtNode(lua_State* L)
{
    DialogueRunner3D* dr = CHECK_DIALOGUE_RUNNER_3D(L, 1);
    const char* nodeId = CHECK_STRING(L, 2);
    dr->StartDialogueAtNode(nodeId ? nodeId : "");
    return 0;
}

int DialogueRunner3D_Lua::StopDialogue(lua_State* L)
{
    DialogueRunner3D* dr = CHECK_DIALOGUE_RUNNER_3D(L, 1);
    dr->StopDialogue();
    return 0;
}

int DialogueRunner3D_Lua::IsDialogueRunning(lua_State* L)
{
    DialogueRunner3D* dr = CHECK_DIALOGUE_RUNNER_3D(L, 1);
    lua_pushboolean(L, dr->IsDialogueRunning() ? 1 : 0);
    return 1;
}

int DialogueRunner3D_Lua::ContinueDialogue(lua_State* L)
{
    DialogueRunner3D* dr = CHECK_DIALOGUE_RUNNER_3D(L, 1);
    dr->ContinueDialogue();
    return 0;
}

int DialogueRunner3D_Lua::ChooseDialogueOption(lua_State* L)
{
    DialogueRunner3D* dr = CHECK_DIALOGUE_RUNNER_3D(L, 1);
    int32_t idx = (int32_t)CHECK_INTEGER(L, 2);
    if (idx < 0) idx = 0;
    dr->ChooseDialogueOption((uint32_t)idx);
    return 0;
}

int DialogueRunner3D_Lua::GetCurrentSpeaker(lua_State* L)
{
    DialogueRunner3D* dr = CHECK_DIALOGUE_RUNNER_3D(L, 1);
    lua_pushstring(L, dr->GetCurrentSpeaker().c_str());
    return 1;
}

int DialogueRunner3D_Lua::GetCurrentSpeakerName(lua_State* L)
{
    DialogueRunner3D* dr = CHECK_DIALOGUE_RUNNER_3D(L, 1);
    lua_pushstring(L, dr->GetCurrentSpeakerName().c_str());
    return 1;
}

int DialogueRunner3D_Lua::GetCurrentPortrait(lua_State* L)
{
    DialogueRunner3D* dr = CHECK_DIALOGUE_RUNNER_3D(L, 1);
    lua_pushstring(L, dr->GetCurrentPortrait().c_str());
    return 1;
}

int DialogueRunner3D_Lua::GetCurrentText(lua_State* L)
{
    DialogueRunner3D* dr = CHECK_DIALOGUE_RUNNER_3D(L, 1);
    lua_pushstring(L, dr->GetCurrentText().c_str());
    return 1;
}

int DialogueRunner3D_Lua::GetCurrentLocKey(lua_State* L)
{
    DialogueRunner3D* dr = CHECK_DIALOGUE_RUNNER_3D(L, 1);
    lua_pushstring(L, dr->GetCurrentLocKey().c_str());
    return 1;
}

int DialogueRunner3D_Lua::GetNumChoices(lua_State* L)
{
    DialogueRunner3D* dr = CHECK_DIALOGUE_RUNNER_3D(L, 1);
    lua_pushinteger(L, (lua_Integer)dr->GetNumChoices());
    return 1;
}

int DialogueRunner3D_Lua::GetChoiceText(lua_State* L)
{
    DialogueRunner3D* dr = CHECK_DIALOGUE_RUNNER_3D(L, 1);
    int32_t idx = (int32_t)CHECK_INTEGER(L, 2);
    if (idx < 0) idx = 0;
    lua_pushstring(L, dr->GetChoiceText((uint32_t)idx).c_str());
    return 1;
}

int DialogueRunner3D_Lua::GetChoiceId(lua_State* L)
{
    DialogueRunner3D* dr = CHECK_DIALOGUE_RUNNER_3D(L, 1);
    int32_t idx = (int32_t)CHECK_INTEGER(L, 2);
    if (idx < 0) idx = 0;
    lua_pushstring(L, dr->GetChoiceId((uint32_t)idx).c_str());
    return 1;
}

int DialogueRunner3D_Lua::SetBool(lua_State* L)
{
    DialogueRunner3D* dr = CHECK_DIALOGUE_RUNNER_3D(L, 1);
    const char* name = CHECK_STRING(L, 2);
    bool v = CHECK_BOOLEAN(L, 3);
    if (name) dr->SetBool(name, v);
    return 0;
}

int DialogueRunner3D_Lua::GetBool(lua_State* L)
{
    DialogueRunner3D* dr = CHECK_DIALOGUE_RUNNER_3D(L, 1);
    const char* name = CHECK_STRING(L, 2);
    bool def = false;
    if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) def = lua_toboolean(L, 3) != 0;
    lua_pushboolean(L, dr->GetBool(name ? name : "", def) ? 1 : 0);
    return 1;
}

int DialogueRunner3D_Lua::SetInt(lua_State* L)
{
    DialogueRunner3D* dr = CHECK_DIALOGUE_RUNNER_3D(L, 1);
    const char* name = CHECK_STRING(L, 2);
    int32_t v = (int32_t)CHECK_INTEGER(L, 3);
    if (name) dr->SetInt(name, v);
    return 0;
}

int DialogueRunner3D_Lua::GetInt(lua_State* L)
{
    DialogueRunner3D* dr = CHECK_DIALOGUE_RUNNER_3D(L, 1);
    const char* name = CHECK_STRING(L, 2);
    int32_t def = 0;
    if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) def = (int32_t)lua_tointeger(L, 3);
    lua_pushinteger(L, dr->GetInt(name ? name : "", def));
    return 1;
}

int DialogueRunner3D_Lua::SetFloat(lua_State* L)
{
    DialogueRunner3D* dr = CHECK_DIALOGUE_RUNNER_3D(L, 1);
    const char* name = CHECK_STRING(L, 2);
    float v = (float)CHECK_NUMBER(L, 3);
    if (name) dr->SetFloat(name, v);
    return 0;
}

int DialogueRunner3D_Lua::GetFloat(lua_State* L)
{
    DialogueRunner3D* dr = CHECK_DIALOGUE_RUNNER_3D(L, 1);
    const char* name = CHECK_STRING(L, 2);
    float def = 0.0f;
    if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) def = (float)lua_tonumber(L, 3);
    lua_pushnumber(L, dr->GetFloat(name ? name : "", def));
    return 1;
}

int DialogueRunner3D_Lua::SetString(lua_State* L)
{
    DialogueRunner3D* dr = CHECK_DIALOGUE_RUNNER_3D(L, 1);
    const char* name = CHECK_STRING(L, 2);
    const char* v = CHECK_STRING(L, 3);
    if (name) dr->SetString(name, v ? v : "");
    return 0;
}

int DialogueRunner3D_Lua::GetString(lua_State* L)
{
    DialogueRunner3D* dr = CHECK_DIALOGUE_RUNNER_3D(L, 1);
    const char* name = CHECK_STRING(L, 2);
    const char* def = "";
    if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) def = lua_tostring(L, 3);
    std::string out = dr->GetString(name ? name : "", def ? def : "");
    lua_pushstring(L, out.c_str());
    return 1;
}

void DialogueRunner3D_Lua::Bind()
{
    lua_State* L = GetLua();
    const int startTop = lua_gettop(L);

    int mtIndex = CreateClassMetatable(
        DIALOGUE_RUNNER_3D_LUA_NAME,
        DIALOGUE_RUNNER_3D_LUA_FLAG,
        NODE_3D_LUA_NAME);

    Node_Lua::BindCommon(L, mtIndex);

    REGISTER_TABLE_FUNC(L, mtIndex, SetDialogueAsset);
    REGISTER_TABLE_FUNC(L, mtIndex, GetDialogueAsset);

    REGISTER_TABLE_FUNC(L, mtIndex, StartDialogue);
    REGISTER_TABLE_FUNC(L, mtIndex, StartDialogueAtNode);
    REGISTER_TABLE_FUNC(L, mtIndex, StopDialogue);
    REGISTER_TABLE_FUNC(L, mtIndex, IsDialogueRunning);

    REGISTER_TABLE_FUNC(L, mtIndex, ContinueDialogue);
    REGISTER_TABLE_FUNC(L, mtIndex, ChooseDialogueOption);

    REGISTER_TABLE_FUNC(L, mtIndex, GetCurrentSpeaker);
    REGISTER_TABLE_FUNC(L, mtIndex, GetCurrentSpeakerName);
    REGISTER_TABLE_FUNC(L, mtIndex, GetCurrentText);
    REGISTER_TABLE_FUNC(L, mtIndex, GetCurrentLocKey);
    REGISTER_TABLE_FUNC(L, mtIndex, GetCurrentPortrait);
    REGISTER_TABLE_FUNC(L, mtIndex, GetNumChoices);
    REGISTER_TABLE_FUNC(L, mtIndex, GetChoiceText);
    REGISTER_TABLE_FUNC(L, mtIndex, GetChoiceId);

    REGISTER_TABLE_FUNC(L, mtIndex, SetBool);
    REGISTER_TABLE_FUNC(L, mtIndex, GetBool);
    REGISTER_TABLE_FUNC(L, mtIndex, SetInt);
    REGISTER_TABLE_FUNC(L, mtIndex, GetInt);
    REGISTER_TABLE_FUNC(L, mtIndex, SetFloat);
    REGISTER_TABLE_FUNC(L, mtIndex, GetFloat);
    REGISTER_TABLE_FUNC(L, mtIndex, SetString);
    REGISTER_TABLE_FUNC(L, mtIndex, GetString);

    lua_pop(L, 1);
    OCT_ASSERT(lua_gettop(L) == startTop);
}

#endif
