#pragma once

#include "Nodes/3D/Node3d.h"
#include "AssetRef.h"

#include "Runtime/DialogueRunner.hxx"
#include "Runtime/DialogueVariableStore.hxx"

#include <cstdint>
#include <string>
#include <vector>

class DialogueAsset;

// Scene-graph wrapper around DialogueRunner. Translates runner state-change
// callbacks into Polyphase signals so Lua / NodeGraph / native nodes can
// subscribe; serializes per-runner variable state and current node id alongside
// the rest of the scene save data.
//
// Defined in an addon DLL — not POLYPHASE_API.
class DialogueRunner3D : public Node3D
{
public:

    DECLARE_NODE(DialogueRunner3D, Node3D);

    DialogueRunner3D();
    virtual ~DialogueRunner3D();

    virtual const char* GetTypeName() const override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;

    virtual void Create() override;
    virtual void Destroy() override;
    virtual void Start() override;
    virtual void Tick(float deltaTime) override;

    virtual void SaveStream(Stream& stream, Platform platform) override;
    virtual void LoadStream(Stream& stream, Platform platform, uint32_t version) override;

    // Asset binding
    void SetDialogueAsset(DialogueAsset* asset);
    DialogueAsset* GetDialogueAsset() const;

    // Lifecycle
    void StartDialogue();
    void StartDialogueAtNode(const std::string& nodeId);
    void StopDialogue();
    bool IsDialogueRunning() const { return mRunner.IsRunning(); }

    // Flow control
    void ContinueDialogue();
    void ChooseDialogueOption(uint32_t index);
    void ChooseDialogueOptionById(const std::string& choiceId);

    // Queries — used by the DialogueBoxWidget and Lua bindings.
    const std::string& GetCurrentSpeaker() const     { return mCurrentSpeakerId; }
    const std::string& GetCurrentSpeakerName() const { return mCurrentSpeakerName; }
    const std::string& GetCurrentText() const        { return mCurrentText; }
    const std::string& GetCurrentLocKey() const      { return mCurrentLocKey; }
    // Resolved portrait asset name for the current line. Per-node `portrait`
    // overrides the speaker's default; named "expressions" are just distinct
    // asset names (e.g. `Guard_Happy`, `Guard_Angry`).
    const std::string& GetCurrentPortrait() const    { return mCurrentPortraitAssetName; }
    uint32_t           GetNumChoices() const         { return (uint32_t)mAvailableChoices.size(); }
    const std::string& GetChoiceText(uint32_t i) const;
    const std::string& GetChoiceId(uint32_t i) const;

    // Per-runner variable accessors (forwarded to mVariableStore).
    void        SetBool  (const std::string& name, bool v);
    void        SetInt   (const std::string& name, int32_t v);
    void        SetFloat (const std::string& name, float v);
    void        SetString(const std::string& name, const std::string& v);
    bool        GetBool  (const std::string& name, bool def = false) const;
    int32_t     GetInt   (const std::string& name, int32_t def = 0) const;
    float       GetFloat (const std::string& name, float def = 0.0f) const;
    std::string GetString(const std::string& name, const std::string& def = std::string()) const;

    // Slot-style state I/O. Round-trips through Stream so the wire format
    // matches what the engine save system uses.
    void SaveDialogueState(std::vector<uint8_t>& outBytes) const;
    bool LoadDialogueState(const std::vector<uint8_t>& bytes);

    static bool HandlePropChange(Datum* datum, uint32_t index, const void* newValue);

protected:

    void RebindRunner();
    void EmitDialogueError(const std::string& message);
    void RebuildCurrentLineCache(const DialogueNodeData& node);
    void RebuildAvailableChoices();

    // Properties (serialized / editor-exposed)
    AssetRef    mDialogueAsset;          // typed accessor: mDialogueAsset.Get<DialogueAsset>()
    bool        mAutoStart  = false;
    bool        mUseGlobalStore = true;  // when true, Set/Get globals fall through to the addon-wide store

    // Runtime state
    DialogueVariableStore mVariableStore;
    DialogueRunner        mRunner;

    // Cached for the widget layer; refreshed in OnLineChanged / OnChoicesChanged.
    std::string mCurrentSpeakerId;
    std::string mCurrentSpeakerName;        // resolved from graph.mSpeakers
    std::string mCurrentText;
    std::string mCurrentLocKey;
    std::string mCurrentPortraitAssetName;  // resolved per-node override + speaker default
    std::vector<DialogueChoiceData> mAvailableChoices;

    // Defer-to-Tick state (mirrors VideoPlayer3D's mPending* gates so signal
    // listeners wired in their own Start() catch the first event).
    bool        mPendingAutoStart = false;
    bool        mStarted          = false;
};
