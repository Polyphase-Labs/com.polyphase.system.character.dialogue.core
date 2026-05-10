#include "Nodes/DialogueRunner3D.hxx"

#include "Assets/DialogueAsset.hxx"
#include "Runtime/DialogueManager.hxx"
#include "Runtime/DialogueEvents.hxx"
#include "EngineAPIAccess.hxx"

#include "Engine.h"
#include "Log.h"
#include "Stream.h"
#include "Datum.h"
#include "Property.h"
#include "Plugins/PolyphaseEngineAPI.h"

#include <cstring>

FORCE_LINK_DEF(DialogueRunner3D);
DEFINE_NODE(DialogueRunner3D, Node3D);

namespace
{
    const std::string kEmptyString;
}

DialogueRunner3D::DialogueRunner3D()
{
    mName = "DialogueRunner";
}

DialogueRunner3D::~DialogueRunner3D()
{
}

const char* DialogueRunner3D::GetTypeName() const
{
    return "DialogueRunner3D";
}

bool DialogueRunner3D::HandlePropChange(Datum* datum, uint32_t /*index*/, const void* newValue)
{
    Property* prop = static_cast<Property*>(datum);
    DialogueRunner3D* self = static_cast<DialogueRunner3D*>(prop->mOwner);
    if (self == nullptr) return false;

    if (prop->mName == "Dialogue Asset")
    {
        Asset* a = *reinterpret_cast<Asset* const*>(newValue);
        self->SetDialogueAsset(static_cast<DialogueAsset*>(a));
        return true;
    }
    return false;
}

void DialogueRunner3D::GatherProperties(std::vector<Property>& outProps)
{
    Node3D::GatherProperties(outProps);

    SCOPED_CATEGORY("Dialogue");

    outProps.push_back(Property(DatumType::Asset, "Dialogue Asset", this, &mDialogueAsset, 1, HandlePropChange,
                                int32_t(DialogueAsset::GetStaticType())));
    outProps.push_back(Property(DatumType::Bool,  "Auto Start",      this, &mAutoStart));
    outProps.push_back(Property(DatumType::Bool,  "Use Global Store",this, &mUseGlobalStore));
}

void DialogueRunner3D::Create()
{
    Node3D::Create();
    DialogueAddon::DialogueManager::Get().RegisterRunner(this);
    RebindRunner();
}

void DialogueRunner3D::Destroy()
{
    StopDialogue();
    DialogueAddon::DialogueManager::Get().UnregisterRunner(this);
    Node3D::Destroy();
}

void DialogueRunner3D::Start()
{
    Node3D::Start();
    mStarted = true;

    // Defer to first Tick so signal listeners wired in their own Start() catch
    // OnDialogueStarted / OnLineChanged. Mirrors VideoPlayer3D's pattern.
    if (mAutoStart && mDialogueAsset.Get() != nullptr)
    {
        mPendingAutoStart = true;
    }
}

void DialogueRunner3D::Tick(float deltaTime)
{
    Node3D::Tick(deltaTime);

    if (mPendingAutoStart)
    {
        mPendingAutoStart = false;
        StartDialogue();
    }
}

void DialogueRunner3D::SetDialogueAsset(DialogueAsset* asset)
{
    if (asset == mDialogueAsset.Get<DialogueAsset>()) return;
    if (mRunner.IsRunning())
    {
        StopDialogue();
    }
    mDialogueAsset = asset;
    RebindRunner();
}

DialogueAsset* DialogueRunner3D::GetDialogueAsset() const
{
    return mDialogueAsset.Get<DialogueAsset>();
}

void DialogueRunner3D::RebindRunner()
{
    mRunner.SetDialogueAsset(mDialogueAsset.Get<DialogueAsset>());
    mRunner.SetVariableStore(&mVariableStore);
    if (mUseGlobalStore)
    {
        mRunner.SetGlobalVariableStore(&DialogueAddon::DialogueManager::Get().GetGlobalStore());
    }
    else
    {
        mRunner.SetGlobalVariableStore(nullptr);
    }

    // Wire callbacks. The runner stores std::function copies — these are owned
    // by the runner and released when this node is destroyed (the lambdas
    // capture `this` and the ScriptFunc-side dispatch is done via EmitSignal,
    // so no Lua refs are stored here).
    DialogueRunner::Callbacks cb;
    cb.OnLineChanged = [this](const DialogueNodeData& n)
    {
        RebuildCurrentLineCache(n);
        std::vector<Datum> args;
        args.emplace_back(Datum(n.mSpeakerId));
        args.emplace_back(Datum(n.mText));
        args.emplace_back(Datum(n.mLocalizationKey));
        EmitSignal("OnLineChanged", args);
    };
    cb.OnChoicesChanged = [this](uint32_t numChoices)
    {
        RebuildAvailableChoices();
        std::vector<Datum> args;
        args.emplace_back(Datum((int32_t)numChoices));
        EmitSignal("OnChoicesChanged", args);
    };
    cb.OnChoiceSelected = [this](const DialogueChoiceData& c)
    {
        std::vector<Datum> args;
        args.emplace_back(Datum(c.mId));
        args.emplace_back(Datum(c.mText));
        EmitSignal("OnChoiceSelected", args);
    };
    cb.OnEvent = [this](const std::string& eventName)
    {
        const std::string assetName = (mDialogueAsset.Get() != nullptr) ? mDialogueAsset.Get()->GetName() : std::string();
        DialogueAddon::DialogueEventDispatcher::Get().Fire(eventName, assetName);
        std::vector<Datum> args;
        args.emplace_back(Datum(eventName));
        EmitSignal("OnDialogueEvent", args);
    };
    cb.OnStarted = [this]()
    {
        EmitSignal("OnDialogueStarted", {});
    };
    cb.OnStopped = [this]()
    {
        mCurrentSpeakerId.clear();
        mCurrentSpeakerName.clear();
        mCurrentText.clear();
        mCurrentLocKey.clear();
        mCurrentPortraitAssetName.clear();
        mAvailableChoices.clear();
        EmitSignal("OnDialogueStopped", {});
    };
    cb.OnFinished = [this]()
    {
        mCurrentSpeakerId.clear();
        mCurrentSpeakerName.clear();
        mCurrentText.clear();
        mCurrentLocKey.clear();
        mCurrentPortraitAssetName.clear();
        mAvailableChoices.clear();
        EmitSignal("OnDialogueFinished", {});
    };
    cb.OnError = [this](const std::string& message)
    {
        EmitDialogueError(message);
    };
    mRunner.SetCallbacks(std::move(cb));
}

void DialogueRunner3D::EmitDialogueError(const std::string& message)
{
    LogError("DialogueRunner3D '%s': %s", mName.c_str(), message.c_str());
    std::vector<Datum> args;
    args.emplace_back(Datum(message));
    EmitSignal("OnDialogueError", args);
}

void DialogueRunner3D::RebuildCurrentLineCache(const DialogueNodeData& node)
{
    mCurrentSpeakerId = node.mSpeakerId;
    mCurrentText      = node.mText;
    mCurrentLocKey    = node.mLocalizationKey;

    // Resolve the speaker's display name and default portrait from the
    // graph's speaker table. Per-node overrides on the line itself win over
    // the speaker default.
    mCurrentSpeakerName.clear();
    mCurrentPortraitAssetName.clear();

    DialogueAsset* asset = mDialogueAsset.Get<DialogueAsset>();
    if (asset != nullptr && !node.mSpeakerId.empty())
    {
        for (const auto& sp : asset->GetGraph().mSpeakers)
        {
            if (sp.mId == node.mSpeakerId)
            {
                mCurrentSpeakerName       = sp.mName.empty() ? sp.mId : sp.mName;
                mCurrentPortraitAssetName = sp.mDefaultPortraitAssetName;
                break;
            }
        }
    }
    if (mCurrentSpeakerName.empty()) mCurrentSpeakerName = node.mSpeakerId;

    // Per-node portrait override: e.g. `Guard_Angry` for one specific line
    // when the speaker's default is `Guard_Default`.
    if (!node.mPortraitAssetName.empty())
    {
        mCurrentPortraitAssetName = node.mPortraitAssetName;
    }
}

void DialogueRunner3D::RebuildAvailableChoices()
{
    mAvailableChoices = mRunner.GetAvailableChoices();
}

void DialogueRunner3D::StartDialogue()
{
    if (mDialogueAsset.Get() == nullptr)
    {
        EmitDialogueError("StartDialogue: no DialogueAsset bound");
        return;
    }
    mRunner.Start();
}

void DialogueRunner3D::StartDialogueAtNode(const std::string& nodeId)
{
    if (mDialogueAsset.Get() == nullptr)
    {
        EmitDialogueError("StartDialogueAtNode: no DialogueAsset bound");
        return;
    }
    mRunner.StartAtNode(nodeId);
}

void DialogueRunner3D::StopDialogue()
{
    mRunner.Stop();
}

void DialogueRunner3D::ContinueDialogue()
{
    if (!mRunner.Continue())
    {
        // Continue is a no-op when waiting on user input; this is expected
        // (the widget calls it whenever the player presses advance).
    }
}

void DialogueRunner3D::ChooseDialogueOption(uint32_t index)
{
    if (!mRunner.Choose(index))
    {
        EmitDialogueError("ChooseDialogueOption: index out of range");
    }
}

void DialogueRunner3D::ChooseDialogueOptionById(const std::string& choiceId)
{
    if (!mRunner.ChooseById(choiceId))
    {
        EmitDialogueError("ChooseDialogueOptionById: unknown choice id '" + choiceId + "'");
    }
}

const std::string& DialogueRunner3D::GetChoiceText(uint32_t i) const
{
    if (i >= mAvailableChoices.size()) return kEmptyString;
    return mAvailableChoices[i].mText;
}

const std::string& DialogueRunner3D::GetChoiceId(uint32_t i) const
{
    if (i >= mAvailableChoices.size()) return kEmptyString;
    return mAvailableChoices[i].mId;
}

void DialogueRunner3D::SetBool(const std::string& name, bool v)
{
    mVariableStore.SetBool(name, v);
}

void DialogueRunner3D::SetInt(const std::string& name, int32_t v)
{
    mVariableStore.SetInt(name, v);
}

void DialogueRunner3D::SetFloat(const std::string& name, float v)
{
    mVariableStore.SetFloat(name, v);
}

void DialogueRunner3D::SetString(const std::string& name, const std::string& v)
{
    mVariableStore.SetString(name, v);
}

bool DialogueRunner3D::GetBool(const std::string& name, bool def) const
{
    if (mVariableStore.HasValue(name)) return mVariableStore.GetBool(name, def);
    if (mUseGlobalStore)
    {
        return DialogueAddon::DialogueManager::Get().GetGlobalStore().GetBool(name, def);
    }
    return def;
}

int32_t DialogueRunner3D::GetInt(const std::string& name, int32_t def) const
{
    if (mVariableStore.HasValue(name)) return mVariableStore.GetInt(name, def);
    if (mUseGlobalStore)
    {
        return DialogueAddon::DialogueManager::Get().GetGlobalStore().GetInt(name, def);
    }
    return def;
}

float DialogueRunner3D::GetFloat(const std::string& name, float def) const
{
    if (mVariableStore.HasValue(name)) return mVariableStore.GetFloat(name, def);
    if (mUseGlobalStore)
    {
        return DialogueAddon::DialogueManager::Get().GetGlobalStore().GetFloat(name, def);
    }
    return def;
}

std::string DialogueRunner3D::GetString(const std::string& name, const std::string& def) const
{
    if (mVariableStore.HasValue(name)) return mVariableStore.GetString(name, def);
    if (mUseGlobalStore)
    {
        return DialogueAddon::DialogueManager::Get().GetGlobalStore().GetString(name, def);
    }
    return def;
}

void DialogueRunner3D::SaveStream(Stream& stream, Platform platform)
{
    Node3D::SaveStream(stream, platform);
    mRunner.SaveStream(stream);
    mVariableStore.SaveStream(stream);
}

void DialogueRunner3D::LoadStream(Stream& stream, Platform platform, uint32_t version)
{
    Node3D::LoadStream(stream, platform, version);
    mRunner.LoadStream(stream);
    mVariableStore.LoadStream(stream);
    // Note: callbacks are wired in Create()/RebindRunner(); LoadStream restores
    // mCurrentNodeId + mRunning but won't re-emit OnLineChanged. The widget
    // re-queries via GetCurrentText / GetCurrentSpeaker on its first Tick.
}

void DialogueRunner3D::SaveDialogueState(std::vector<uint8_t>& outBytes) const
{
    Stream s;
    mRunner.SaveStream(s);
    mVariableStore.SaveStream(s);
    const uint32_t sz = s.GetSize();
    outBytes.resize(sz);
    if (sz > 0)
    {
        std::memcpy(outBytes.data(), s.GetData(), sz);
    }
}

bool DialogueRunner3D::LoadDialogueState(const std::vector<uint8_t>& bytes)
{
    if (bytes.empty()) return false;
    Stream s((const char*)bytes.data(), (uint32_t)bytes.size());
    s.SetPos(0);
    mRunner.LoadStream(s);
    mVariableStore.LoadStream(s);
    return true;
}
