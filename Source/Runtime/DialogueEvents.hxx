#pragma once

#include "ScriptFunc.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace DialogueAddon
{
    // Native event handler signature. Used by character-system addons to react
    // to dialogue Event nodes whose name is of the form "Fn(arg1,arg2,...)".
    // Args are the comma-split contents inside the parens (whitespace trimmed,
    // surrounding double-quotes stripped). Plain event names — no parens —
    // skip native dispatch and only fire the Lua listeners + node signal.
    //
    // userData is whatever the registering addon passed at Connect time;
    // typically a pointer to its singleton manager.
    typedef void (*DialogueNativeEventHandler)(const std::string& eventName,
                                               const std::vector<std::string>& args,
                                               void* userData);

    // Global dialogue-event dispatcher. Lua scripts call Dialogue.OnEvent(name, fn)
    // to subscribe; DialogueRunner3D fires events via the manager so all
    // listeners light up regardless of which conversation produced the event.
    //
    // Mirrors VideoPlayerAddon::PlaylistEventDispatcher so the hot-reload
    // cleanup pattern is identical: Clear() is called from OnUnload before
    // the addon DLL is freed so stored ScriptFunc Lua refs release safely.
    class DialogueEventDispatcher
    {
    public:
        using ListenerId = int32_t;
        static constexpr ListenerId kInvalidId = 0;

        static DialogueEventDispatcher& Get();

        // Connect a Lua callback to a named event. Returns a non-zero handle,
        // or kInvalidId on failure. Callback signature in Lua:
        //   function(eventName, dialogueAssetName)
        ListenerId Connect(const std::string& eventName, const ScriptFunc& fn);

        bool Disconnect(ListenerId id);

        // Dispatch. Listeners are snapshotted under the mutex first so a
        // callback can re-enter Connect/Disconnect safely.
        // args: optional parsed function-call arguments (if the originating
        // event name was "Fn(a,b,c)"). Lua callbacks receive
        //   (eventName, dialogueAssetName, arg1, arg2, ...)
        // so subscribers can route to typed handlers without re-parsing.
        void Fire(const std::string& eventName,
                  const std::string& dialogueAssetName,
                  const std::vector<std::string>& args = {});

        // Drops every listener and every native handler. Called from
        // DialogueCoreAddon.cpp::OnUnload.
        void Clear();

        // --- Native handler API ---
        //
        // Called by character-system addons in their OnLoad to wire dialogue
        // Event nodes (e.g. "TriggerBark(npc_01,annoyed)") into native code.
        // The same eventName may have multiple registered handlers; all fire.
        //
        // ownerId scopes lifetime: each addon passes the hookId it received
        // from RegisterEditorUI (or any non-zero u64 unique to that addon).
        // OnUnload calls UnregisterAllForOwner(myHookId) to release every
        // handler the addon installed before its DLL is freed.
        void RegisterNativeHandler(const std::string& eventName,
                                   DialogueNativeEventHandler handler,
                                   void* userData,
                                   uint64_t ownerId);

        // Drop every native handler registered with this owner id. Safe to
        // call from OnUnload; safe to call multiple times.
        void UnregisterAllForOwner(uint64_t ownerId);

        // Dispatch a parsed function-style event. Called from DialogueRunner3D
        // when an Event node's name parses as "Fn(arg,arg,...)". Snapshot is
        // taken under the mutex so handlers can re-enter Register/Unregister.
        void DispatchNative(const std::string& eventName,
                            const std::vector<std::string>& args);

    private:
        DialogueEventDispatcher() = default;

        struct Entry
        {
            ListenerId  id;
            std::string event;
            ScriptFunc  fn;
        };

        struct NativeEntry
        {
            std::string                event;
            DialogueNativeEventHandler handler  = nullptr;
            void*                      userData = nullptr;
            uint64_t                   ownerId  = 0;
        };

        std::vector<Entry>       mEntries;
        std::vector<NativeEntry> mNativeEntries;
        ListenerId               mNextId = 1;
        std::mutex               mMutex;
    };
}
