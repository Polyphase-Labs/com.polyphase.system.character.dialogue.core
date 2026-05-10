#include "Runtime/DialogueEvents.hxx"

#include "Datum.h"

namespace DialogueAddon
{
    DialogueEventDispatcher& DialogueEventDispatcher::Get()
    {
        static DialogueEventDispatcher sInstance;
        return sInstance;
    }

    DialogueEventDispatcher::ListenerId
    DialogueEventDispatcher::Connect(const std::string& eventName, const ScriptFunc& fn)
    {
        if (eventName.empty() || !fn.IsValid()) return kInvalidId;

        std::lock_guard<std::mutex> lock(mMutex);
        Entry e;
        e.id    = mNextId++;
        e.event = eventName;
        e.fn    = fn;
        mEntries.push_back(std::move(e));
        return mEntries.back().id;
    }

    bool DialogueEventDispatcher::Disconnect(ListenerId id)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        for (auto it = mEntries.begin(); it != mEntries.end(); ++it)
        {
            if (it->id == id)
            {
                mEntries.erase(it);
                return true;
            }
        }
        return false;
    }

    void DialogueEventDispatcher::Fire(const std::string& eventName,
                                       const std::string& dialogueAssetName)
    {
        // Snapshot listeners that match either the specific event name or the
        // wildcard "*". Wildcard subscribers receive every event with the real
        // name as the first argument.
        std::vector<ScriptFunc> callbacks;
        {
            std::lock_guard<std::mutex> lock(mMutex);
            callbacks.reserve(mEntries.size());
            for (const Entry& e : mEntries)
            {
                if (e.event == eventName || e.event == "*")
                {
                    callbacks.push_back(e.fn);
                }
            }
        }
        if (callbacks.empty()) return;

        Datum params[2];
        params[0] = Datum(eventName);
        params[1] = Datum(dialogueAssetName);

        for (const ScriptFunc& fn : callbacks)
        {
            fn.Call(2, params);
        }
    }

    void DialogueEventDispatcher::Clear()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mEntries.clear();
    }
}
