#include "Runtime/DialogueManager.hxx"

#include "Nodes/DialogueRunner3D.hxx"

namespace DialogueAddon
{
    DialogueManager& DialogueManager::Get()
    {
        static DialogueManager sInstance;
        return sInstance;
    }

    void DialogueManager::RegisterRunner(DialogueRunner3D* runner)
    {
        if (runner == nullptr) return;
        std::lock_guard<std::mutex> lock(mMutex);
        mRunners.insert(runner);
    }

    void DialogueManager::UnregisterRunner(DialogueRunner3D* runner)
    {
        if (runner == nullptr) return;
        std::lock_guard<std::mutex> lock(mMutex);
        mRunners.erase(runner);
    }

    void DialogueManager::StopAll()
    {
        // Snapshot the set under lock; call StopDialogue without the lock
        // because that path may emit signals which could re-enter the manager
        // (e.g. a Lua handler that calls Dialogue.GetGlobalBool).
        std::vector<DialogueRunner3D*> snapshot;
        {
            std::lock_guard<std::mutex> lock(mMutex);
            snapshot.reserve(mRunners.size());
            for (auto* r : mRunners) snapshot.push_back(r);
        }
        for (auto* r : snapshot)
        {
            if (r != nullptr) r->StopDialogue();
        }
    }

    void DialogueManager::Clear()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mRunners.clear();
        mGlobalStore.Clear();
    }
}
