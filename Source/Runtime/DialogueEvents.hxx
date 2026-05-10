#pragma once

#include "ScriptFunc.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace DialogueAddon
{
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
        void Fire(const std::string& eventName,
                  const std::string& dialogueAssetName);

        // Drops every listener. Called from DialogueCoreAddon.cpp::OnUnload.
        void Clear();

    private:
        DialogueEventDispatcher() = default;

        struct Entry
        {
            ListenerId  id;
            std::string event;
            ScriptFunc  fn;
        };

        std::vector<Entry> mEntries;
        ListenerId         mNextId = 1;
        std::mutex         mMutex;
    };
}
