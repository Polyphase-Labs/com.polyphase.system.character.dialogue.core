/**
 * @file DialogueCoreAddon.cpp
 * @brief Native addon: com.polyphase.system.character.dialogue.core
 *
 * Plugin entry. Registers DialogueAsset / DialogueRunner3D / DialogueBoxWidget
 * with the engine factories via FORCE_LINK_CALL, wires the .dialogue.json
 * import extension, binds Lua, and registers editor UI hooks.
 *
 * Mirrors the exact OnLoad / OnUnload / FillDesc / dual-entry-point pattern
 * used by VideoPlayer.cpp.
 */

#include "Plugins/PolyphasePluginAPI.h"
#include "Plugins/PolyphaseEngineAPI.h"

#include "EngineAPIAccess.hxx"
#include "Assets/DialogueAsset.hxx"
#include "Nodes/DialogueRunner3D.hxx"
#include "Widgets/DialogueBoxWidget.hxx"
#include "Lua/Dialogue_Lua.hxx"
#include "Lua/DialogueRunner3D_Lua.hxx"
#include "Runtime/DialogueManager.hxx"
#include "Runtime/DialogueEvents.hxx"

#if EDITOR
#include "AssetManager.h"
#include "Plugins/EditorUIHooks.h"
#include "Editor/DialogueAssetViewer.hxx"
#include "Editor/DialoguePreviewWindow.hxx"
#endif

namespace DialogueAddon
{
    static PolyphaseEngineAPI* sEngineAPI = nullptr;
    PolyphaseEngineAPI* GetEngineAPI()                 { return sEngineAPI; }
    void                SetEngineAPI(PolyphaseEngineAPI* api) { sEngineAPI = api; }
}

static int OnLoad(PolyphaseEngineAPI* api)
{
    DialogueAddon::SetEngineAPI(api);

    // Pull addon types into the link so DECLARE_ASSET / DECLARE_NODE static
    // initializers register with the engine factories. Without this the
    // optimiser may discard translation units that nothing else references.
    FORCE_LINK_CALL(DialogueAsset);
    FORCE_LINK_CALL(DialogueRunner3D);
    FORCE_LINK_CALL(DialogueBoxWidget);

#if EDITOR
    // Editor's import dispatcher (ActionManager::ImportAsset) keys on the
    // file's last `.` segment, so register `.dialogue`. The asset payload is
    // still JSON — point your editor's syntax highlighter at .dialogue.
    RegisterImportExtension(".dialogue", DialogueAsset::GetStaticType());
#endif

    if (api != nullptr && api->LogDebug != nullptr)
    {
        api->LogDebug("dialogue.core addon loaded");
    }
    return 0;
}

static void OnUnload()
{
    // Hot-reload safety: drop everything the addon owns BEFORE the DLL is
    // freed.
    //  - DialogueManager::StopAll: stops live runners that may emit signals
    //    to Lua handlers.
    //  - DialogueManager::Clear: drops the runner-tracking set and the global
    //    variable store.
    //  - DialogueEventDispatcher::Clear: releases ScriptFunc Lua refs so they
    //    don't stay registered against a dead DLL.
    //  - ResetDialoguePreview (editor only): clears the preview window's
    //    transient asset pointer and cached strings.
    DialogueAddon::DialogueManager::Get().StopAll();
    DialogueAddon::DialogueManager::Get().Clear();
    DialogueAddon::DialogueEventDispatcher::Get().Clear();

#if EDITOR
    DialogueAddon::ResetDialoguePreview();
#endif

    PolyphaseEngineAPI* api = DialogueAddon::GetEngineAPI();
    if (api != nullptr && api->LogDebug != nullptr)
    {
        api->LogDebug("dialogue.core addon unloaded");
    }
    DialogueAddon::SetEngineAPI(nullptr);
}

static void RegisterTypes(void* /*nodeFactory*/)
{
    // Custom types register via DECLARE_NODE / DECLARE_ASSET static
    // initializers. No explicit work needed.
}

static void RegisterScriptFuncs(lua_State* /*L*/)
{
#if LUA_ENABLED
    DialogueRunner3D_Lua::Bind();
    Dialogue_Lua::Bind();
#endif
}

#if EDITOR
static void RegisterEditorUI(EditorUIHooks* hooks, uint64_t hookId)
{
    if (hooks == nullptr) return;

    // Inspector extension for DialogueAsset — adds Validate / Open Preview
    // buttons under the standard property grid.
    if (hooks->RegisterInspector != nullptr)
    {
        hooks->RegisterInspector(hookId, "DialogueAsset",
                                 &DialogueAddon::DialogueAssetInspector, nullptr);
    }

    // Dockable preview window.
    if (hooks->RegisterWindow != nullptr)
    {
        hooks->RegisterWindow(hookId, "Dialogue Preview",
                              "polyphase.dialogue.preview",
                              &DialogueAddon::DialoguePreviewDraw, nullptr);
    }

    // Tools menu entry.
    if (hooks->AddMenuItem != nullptr)
    {
        struct MenuLambda
        {
            static void Open(void* /*userData*/)
            {
                PolyphaseEngineAPI* api = DialogueAddon::GetEngineAPI();
                if (api != nullptr && api->editorUI != nullptr && api->editorUI->OpenWindow != nullptr)
                {
                    api->editorUI->OpenWindow("polyphase.dialogue.preview");
                }
            }
        };
        hooks->AddMenuItem(hookId, "Tools", "Dialogue/Open Preview",
                           &MenuLambda::Open, nullptr, nullptr);
    }
}
#endif

static int FillDesc(PolyphasePluginDesc* desc)
{
    desc->apiVersion = OCTAVE_PLUGIN_API_VERSION;
    desc->pluginName = "DialogueCore";
    desc->pluginVersion = "0.1.0";
    desc->OnLoad = OnLoad;
    desc->OnUnload = OnUnload;
    desc->Tick = nullptr;
    desc->TickEditor = nullptr;
    desc->RegisterTypes = RegisterTypes;
    desc->RegisterScriptFuncs = RegisterScriptFuncs;
#if EDITOR
    desc->RegisterEditorUI = RegisterEditorUI;
#else
    desc->RegisterEditorUI = nullptr;
#endif
    desc->OnEditorPreInit = nullptr;
    desc->OnEditorReady = nullptr;
    return 0;
}

#if EDITOR
extern "C" OCTAVE_PLUGIN_API int PolyphasePlugin_GetDesc(PolyphasePluginDesc* desc)
{
    return FillDesc(desc);
}
#else
// Shipped build: unique entry-point name (dots in the addon id replaced with
// underscores) so multiple addons coexist in the exe without collisions. The
// editor's "Regenerate Native Addon Dependencies" tool emits the matching
// POLYPHASE_REGISTER_PLUGIN line.
extern "C" int PolyphasePlugin_GetDesc_com_polyphase_system_character_dialogue_core(
    PolyphasePluginDesc* desc)
{
    return FillDesc(desc);
}
#endif
