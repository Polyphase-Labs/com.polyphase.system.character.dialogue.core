#include "Assets/DialogueAsset.hxx"
#include "Assets/DialogueJson.hxx"

#include "Log.h"
#include "Stream.h"
#include "Property.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>

FORCE_LINK_DEF(DialogueAsset);
DEFINE_ASSET(DialogueAsset);

namespace
{
    // On-disk format version. Bump only when the layout below changes in a way
    // older readers can't tolerate. Bytes appended after the trailer magic do
    // NOT require a bump — they're presence-checked individually, mirroring
    // VideoClip's cook-trailer pattern.
    constexpr uint32_t kDialogueAssetVersion = 1;

    // Magic uint32 anchoring the trailer block. Anything after this is
    // forward-compat fields.
    constexpr uint32_t kDialogueTrailerMagic = 0xD1A107C0u;

    // ---- Value primitives -------------------------------------------------

    void WriteValue(Stream& s, const DialogueValue& v)
    {
        s.WriteUint8((uint8_t)v.mType);
        s.WriteBool(v.mBool);
        s.WriteInt32(v.mInt);
        s.WriteFloat(v.mFloat);
        s.WriteString(v.mString);
    }

    void ReadValue(Stream& s, DialogueValue& v)
    {
        v.mType  = (DialogueValueType)s.ReadUint8();
        v.mBool  = s.ReadBool();
        v.mInt   = s.ReadInt32();
        v.mFloat = s.ReadFloat();
        s.ReadString(v.mString);
    }

    // ---- Conditions / variable ops ----------------------------------------

    void WriteCondition(Stream& s, const DialogueConditionData& c)
    {
        s.WriteString(c.mVariableName);
        s.WriteUint8((uint8_t)c.mOp);
        WriteValue(s, c.mValue);
    }

    void ReadCondition(Stream& s, DialogueConditionData& c)
    {
        s.ReadString(c.mVariableName);
        c.mOp = (DialogueConditionOp)s.ReadUint8();
        ReadValue(s, c.mValue);
    }

    void WriteConditionList(Stream& s, const std::vector<DialogueConditionData>& v)
    {
        s.WriteUint32((uint32_t)v.size());
        for (const auto& c : v) WriteCondition(s, c);
    }

    void ReadConditionList(Stream& s, std::vector<DialogueConditionData>& v)
    {
        uint32_t n = s.ReadUint32();
        v.clear();
        v.reserve(n);
        for (uint32_t i = 0; i < n; ++i)
        {
            DialogueConditionData c;
            ReadCondition(s, c);
            v.emplace_back(std::move(c));
        }
    }

    void WriteStringList(Stream& s, const std::vector<std::string>& v)
    {
        s.WriteUint32((uint32_t)v.size());
        for (const auto& str : v) s.WriteString(str);
    }

    void ReadStringList(Stream& s, std::vector<std::string>& v)
    {
        uint32_t n = s.ReadUint32();
        v.clear();
        v.reserve(n);
        for (uint32_t i = 0; i < n; ++i)
        {
            std::string str;
            s.ReadString(str);
            v.emplace_back(std::move(str));
        }
    }

    // ---- Choice -----------------------------------------------------------

    void WriteChoice(Stream& s, const DialogueChoiceData& c)
    {
        s.WriteString(c.mId);
        s.WriteString(c.mText);
        s.WriteString(c.mLocalizationKey);
        s.WriteString(c.mTargetNodeId);
        WriteConditionList(s, c.mConditions);
        WriteStringList(s, c.mTags);
    }

    void ReadChoice(Stream& s, DialogueChoiceData& c)
    {
        s.ReadString(c.mId);
        s.ReadString(c.mText);
        s.ReadString(c.mLocalizationKey);
        s.ReadString(c.mTargetNodeId);
        ReadConditionList(s, c.mConditions);
        ReadStringList(s, c.mTags);
    }

    // ---- Node -------------------------------------------------------------

    void WriteNode(Stream& s, const DialogueNodeData& n)
    {
        s.WriteString(n.mId);
        s.WriteUint8((uint8_t)n.mType);
        s.WriteString(n.mSpeakerId);
        s.WriteString(n.mText);
        s.WriteString(n.mLocalizationKey);
        s.WriteString(n.mPortraitAssetName);
        s.WriteString(n.mVoiceAssetName);
        s.WriteString(n.mEventName);
        s.WriteString(n.mJumpTargetId);

        WriteStringList(s, n.mTags);
        WriteConditionList(s, n.mConditions);

        s.WriteUint32((uint32_t)n.mChoices.size());
        for (const auto& c : n.mChoices) WriteChoice(s, c);

        // Variable op
        s.WriteUint8((uint8_t)n.mVariableOp.mOp);
        s.WriteString(n.mVariableOp.mVariableName);
        WriteValue(s, n.mVariableOp.mValue);
    }

    void ReadNode(Stream& s, DialogueNodeData& n)
    {
        s.ReadString(n.mId);
        n.mType = (DialogueNodeType)s.ReadUint8();
        s.ReadString(n.mSpeakerId);
        s.ReadString(n.mText);
        s.ReadString(n.mLocalizationKey);
        s.ReadString(n.mPortraitAssetName);
        s.ReadString(n.mVoiceAssetName);
        s.ReadString(n.mEventName);
        s.ReadString(n.mJumpTargetId);

        ReadStringList(s, n.mTags);
        ReadConditionList(s, n.mConditions);

        uint32_t numChoices = s.ReadUint32();
        n.mChoices.clear();
        n.mChoices.reserve(numChoices);
        for (uint32_t i = 0; i < numChoices; ++i)
        {
            DialogueChoiceData c;
            ReadChoice(s, c);
            n.mChoices.emplace_back(std::move(c));
        }

        n.mVariableOp.mOp = (DialogueVarOp)s.ReadUint8();
        s.ReadString(n.mVariableOp.mVariableName);
        ReadValue(s, n.mVariableOp.mValue);
    }
}

DialogueAsset::DialogueAsset()
{
    mType = DialogueAsset::GetStaticType();
}

DialogueAsset::~DialogueAsset()
{
}

void DialogueAsset::Create()
{
    Asset::Create();
}

void DialogueAsset::Destroy()
{
    Asset::Destroy();
    mGraph = DialogueGraphData{};
}

const char* DialogueAsset::GetTypeName()
{
    return "DialogueAsset";
}

const char* DialogueAsset::GetTypeImportExt()
{
    return ".dialogue";
}

glm::vec4 DialogueAsset::GetTypeColor()
{
    // Soft purple — distinct from VideoClip's color and the engine's defaults.
    return glm::vec4(0.55f, 0.45f, 0.85f, 1.0f);
}

const DialogueNodeData* DialogueAsset::FindNode(const std::string& id) const
{
    for (const auto& n : mGraph.mNodes)
    {
        if (n.mId == id) return &n;
    }
    return nullptr;
}

bool DialogueAsset::Import(const std::string& path, ImportOptions* options)
{
    OCT_UNUSED(options);

    DialogueGraphData graph;
    std::string err;
    if (!DialogueAddon::ReadDialogueJsonFile(path, graph, err))
    {
        LogError("DialogueAsset::Import: failed to load '%s': %s", path.c_str(), err.c_str());
        return false;
    }

    if (graph.mStartNodeId.empty() && !graph.mNodes.empty())
    {
        graph.mStartNodeId = graph.mNodes.front().mId;
        LogWarning("DialogueAsset::Import: '%s' has no 'start'; defaulted to first node '%s'",
                   path.c_str(), graph.mStartNodeId.c_str());
    }

    mGraph = std::move(graph);
    return true;
}

void DialogueAsset::SaveStream(Stream& stream, Platform platform)
{
    Asset::SaveStream(stream, platform);

    stream.WriteUint32(kDialogueAssetVersion);
    stream.WriteString(mGraph.mStartNodeId);

    // Speakers
    stream.WriteUint32((uint32_t)mGraph.mSpeakers.size());
    for (const auto& sp : mGraph.mSpeakers)
    {
        stream.WriteString(sp.mId);
        stream.WriteString(sp.mName);
        stream.WriteString(sp.mDefaultPortraitAssetName);
        stream.WriteString(sp.mDefaultVoiceAssetName);
    }

    // Variables
    stream.WriteUint32((uint32_t)mGraph.mVariables.size());
    for (const auto& v : mGraph.mVariables)
    {
        stream.WriteString(v.mName);
        stream.WriteUint8((uint8_t)v.mType);
        WriteValue(stream, v.mDefault);
    }

    // Nodes
    stream.WriteUint32((uint32_t)mGraph.mNodes.size());
    for (const auto& n : mGraph.mNodes) WriteNode(stream, n);

    // Links (graph layout / metadata; choice targets and node.jumpTargetId
    // drive the actual flow at runtime).
    stream.WriteUint32((uint32_t)mGraph.mLinks.size());
    for (const auto& l : mGraph.mLinks)
    {
        stream.WriteString(l.mFromNodeId);
        stream.WriteString(l.mToNodeId);
    }

    // Trailer magic for future fields. Older readers stop after links; newer
    // readers presence-check each appended field.
    stream.WriteUint32(kDialogueTrailerMagic);
}

void DialogueAsset::LoadStream(Stream& stream, Platform platform)
{
    Asset::LoadStream(stream, platform);

    mGraph = DialogueGraphData{};
    mGraph.mVersion = stream.ReadUint32();
    stream.ReadString(mGraph.mStartNodeId);

    uint32_t numSpeakers = stream.ReadUint32();
    mGraph.mSpeakers.clear();
    mGraph.mSpeakers.reserve(numSpeakers);
    for (uint32_t i = 0; i < numSpeakers; ++i)
    {
        DialogueSpeakerDef sp;
        stream.ReadString(sp.mId);
        stream.ReadString(sp.mName);
        stream.ReadString(sp.mDefaultPortraitAssetName);
        stream.ReadString(sp.mDefaultVoiceAssetName);
        mGraph.mSpeakers.emplace_back(std::move(sp));
    }

    uint32_t numVars = stream.ReadUint32();
    mGraph.mVariables.clear();
    mGraph.mVariables.reserve(numVars);
    for (uint32_t i = 0; i < numVars; ++i)
    {
        DialogueVariableDef v;
        stream.ReadString(v.mName);
        v.mType = (DialogueValueType)stream.ReadUint8();
        ReadValue(stream, v.mDefault);
        mGraph.mVariables.emplace_back(std::move(v));
    }

    uint32_t numNodes = stream.ReadUint32();
    mGraph.mNodes.clear();
    mGraph.mNodes.reserve(numNodes);
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        DialogueNodeData n;
        ReadNode(stream, n);
        mGraph.mNodes.emplace_back(std::move(n));
    }

    uint32_t numLinks = stream.ReadUint32();
    mGraph.mLinks.clear();
    mGraph.mLinks.reserve(numLinks);
    for (uint32_t i = 0; i < numLinks; ++i)
    {
        DialogueLinkData l;
        stream.ReadString(l.mFromNodeId);
        stream.ReadString(l.mToNodeId);
        mGraph.mLinks.emplace_back(std::move(l));
    }

    // Optional trailer (matches VideoClip's pattern). Older files lack the
    // magic — the surrounding system's stream-position is preserved either
    // way so subsequent readers don't lose alignment.
    if (stream.GetPos() + 4 <= stream.GetSize())
    {
        uint32_t mark = stream.ReadUint32();
        if (mark != kDialogueTrailerMagic)
        {
            stream.SetPos(stream.GetPos() - 4);
        }
        // No fields after the magic in v1; future versions append here and
        // presence-check via stream.GetPos().
    }
}

void DialogueAsset::GatherProperties(std::vector<Property>& outProps)
{
    Asset::GatherProperties(outProps);

    SCOPED_CATEGORY("Dialogue");

    // Read-only summary. The graph itself is edited via the editor preview /
    // graph viewer, not the inspector grid (inspector can't render arrays of
    // composite structs gracefully).
    static std::string sStartLabel;
    static std::string sNodeCountLabel;
    static std::string sSpeakerCountLabel;
    static std::string sVariableCountLabel;

    char buf[64];

    sStartLabel = mGraph.mStartNodeId;
    outProps.push_back(Property(DatumType::String,  "Start Node",     this, &sStartLabel));

    std::snprintf(buf, sizeof(buf), "%u", (unsigned)mGraph.mNodes.size());
    sNodeCountLabel = buf;
    outProps.push_back(Property(DatumType::String,  "Node Count",     this, &sNodeCountLabel));

    std::snprintf(buf, sizeof(buf), "%u", (unsigned)mGraph.mSpeakers.size());
    sSpeakerCountLabel = buf;
    outProps.push_back(Property(DatumType::String,  "Speaker Count",  this, &sSpeakerCountLabel));

    std::snprintf(buf, sizeof(buf), "%u", (unsigned)mGraph.mVariables.size());
    sVariableCountLabel = buf;
    outProps.push_back(Property(DatumType::String,  "Variable Count", this, &sVariableCountLabel));
}
