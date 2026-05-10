#pragma once

#include "Runtime/DialogueVariableStore.hxx"

#include <cstdint>
#include <mutex>
#include <unordered_set>

class DialogueRunner3D;

// Singleton registry of live DialogueRunner3D instances and the addon-wide
// global variable store. Lifetime is the addon DLL's; DialogueCoreAddon.cpp's
// OnUnload calls StopAll() and Clear() so transient state releases before the
// engine's hot-reload purge runs.
//
// Mirrors VideoPlayerAddon::PlaylistRegistry as a hot-reload safety pattern.
namespace DialogueAddon
{
    class DialogueManager
    {
    public:
        static DialogueManager& Get();

        // Lifecycle from runner nodes — they self-register on Create and
        // self-unregister on Destroy. The manager never owns runners.
        void RegisterRunner(DialogueRunner3D* runner);
        void UnregisterRunner(DialogueRunner3D* runner);

        // StopAll: ask every registered runner to stop. Called from OnUnload
        // before the singleton's containers are cleared so signals fire and
        // listeners can react before their callbacks are released.
        void StopAll();

        // Clear: drop tracking. Does NOT delete the runners — they're owned
        // by the world. Called from OnUnload after StopAll.
        void Clear();

        // Global variable store, shared across all conversations. Useful for
        // game-wide flags (met_npc, completed_quest, etc.) that cross
        // dialogue assets.
        DialogueVariableStore&       GetGlobalStore()        { return mGlobalStore; }
        const DialogueVariableStore& GetGlobalStore() const  { return mGlobalStore; }

    private:
        DialogueManager() = default;

        std::mutex mMutex;
        std::unordered_set<DialogueRunner3D*> mRunners;
        DialogueVariableStore mGlobalStore;
    };
}
