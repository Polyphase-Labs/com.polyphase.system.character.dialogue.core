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
                                       const std::string& dialogueAssetName,
                                       const std::vector<std::string>& args)
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

        // Build params: (eventName, dialogueAssetName, arg1, arg2, ...)
        // Cap args to a reasonable upper bound (16) so a malformed event name
        // can't blow the parameter stack.
        constexpr uint32_t kMaxArgs = 16;
        const uint32_t argCount = (args.size() <= kMaxArgs) ? (uint32_t)args.size() : kMaxArgs;
        const uint32_t totalParams = 2 + argCount;

        std::vector<Datum> params;
        params.reserve(totalParams);
        params.emplace_back(Datum(eventName));
        params.emplace_back(Datum(dialogueAssetName));
        for (uint32_t i = 0; i < argCount; ++i)
        {
            params.emplace_back(Datum(args[i]));
        }

        for (const ScriptFunc& fn : callbacks)
        {
            fn.Call(totalParams, params.data());
        }
    }

    void DialogueEventDispatcher::Clear()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mEntries.clear();
        mNativeEntries.clear();
    }

    void DialogueEventDispatcher::RegisterNativeHandler(const std::string& eventName,
                                                        DialogueNativeEventHandler handler,
                                                        void* userData,
                                                        uint64_t ownerId)
    {
        if (eventName.empty() || handler == nullptr) return;

        std::lock_guard<std::mutex> lock(mMutex);
        NativeEntry e;
        e.event    = eventName;
        e.handler  = handler;
        e.userData = userData;
        e.ownerId  = ownerId;
        mNativeEntries.push_back(std::move(e));
    }

    void DialogueEventDispatcher::UnregisterAllForOwner(uint64_t ownerId)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        auto it = mNativeEntries.begin();
        while (it != mNativeEntries.end())
        {
            if (it->ownerId == ownerId)
            {
                it = mNativeEntries.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void DialogueEventDispatcher::DispatchNative(const std::string& eventName,
                                                 const std::vector<std::string>& args)
    {
        // Snapshot matching entries under the lock so handlers can re-enter.
        struct Snapshot { DialogueNativeEventHandler fn; void* ud; };
        std::vector<Snapshot> matched;
        {
            std::lock_guard<std::mutex> lock(mMutex);
            matched.reserve(mNativeEntries.size());
            for (const NativeEntry& e : mNativeEntries)
            {
                if (e.event == eventName || e.event == "*")
                {
                    matched.push_back({e.handler, e.userData});
                }
            }
        }
        for (const Snapshot& s : matched)
        {
            s.fn(eventName, args, s.ud);
        }
    }
}
