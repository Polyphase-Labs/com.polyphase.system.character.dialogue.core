#pragma once

#include <cstdint>
#include <string>
#include <vector>

// All types here are POD-only (no virtuals, no inheritance, no exceptions, no
// RTTI dependency). The runtime ships to 3DS / Wii / GameCube where -fno-rtti
// and -fno-exceptions are required.

enum class DialogueNodeType : uint8_t
{
    Line        = 0,
    Choice      = 1,
    Branch      = 2,
    Event       = 3,
    SetVariable = 4,
    Jump        = 5,
    End         = 6,
    Count
};

enum class DialogueValueType : uint8_t
{
    Bool   = 0,
    Int    = 1,
    Float  = 2,
    String = 3,
    Count
};

enum class DialogueConditionOp : uint8_t
{
    Exists         = 0,
    NotExists      = 1,
    Equals         = 2,
    NotEquals      = 3,
    Greater        = 4,
    GreaterOrEqual = 5,
    Less           = 6,
    LessOrEqual    = 7,
    Count
};

enum class DialogueVarOp : uint8_t
{
    None     = 0,
    Set      = 1,
    Add      = 2,
    Sub      = 3,
    Toggle   = 4,
    Count
};

struct DialogueValue
{
    DialogueValueType mType = DialogueValueType::Bool;
    bool        mBool   = false;
    int32_t     mInt    = 0;
    float       mFloat  = 0.0f;
    std::string mString;
};

struct DialogueConditionData
{
    std::string         mVariableName;
    DialogueConditionOp mOp = DialogueConditionOp::Equals;
    DialogueValue       mValue;
};

struct DialogueVariableOpData
{
    DialogueVarOp mOp = DialogueVarOp::None;
    std::string   mVariableName;
    DialogueValue mValue;
};

struct DialogueChoiceData
{
    std::string mId;
    std::string mText;
    std::string mLocalizationKey;
    std::string mTargetNodeId;

    std::vector<DialogueConditionData> mConditions;
    std::vector<std::string>           mTags;
};

struct DialogueNodeData
{
    std::string      mId;
    DialogueNodeType mType = DialogueNodeType::Line;

    std::string mSpeakerId;
    std::string mText;
    std::string mLocalizationKey;
    std::string mPortraitAssetName;
    std::string mVoiceAssetName;
    std::string mEventName;
    std::string mJumpTargetId;

    std::vector<std::string>           mTags;
    std::vector<DialogueConditionData> mConditions;
    std::vector<DialogueChoiceData>    mChoices;

    DialogueVariableOpData mVariableOp;
};

struct DialogueLinkData
{
    std::string mFromNodeId;
    std::string mToNodeId;
};

struct DialogueVariableDef
{
    std::string       mName;
    DialogueValueType mType = DialogueValueType::Bool;
    DialogueValue     mDefault;
};

struct DialogueSpeakerDef
{
    std::string mId;
    std::string mName;
    std::string mDefaultPortraitAssetName;
    std::string mDefaultVoiceAssetName;
};

struct DialogueGraphData
{
    std::string mStartNodeId;

    std::vector<DialogueNodeData>     mNodes;
    std::vector<DialogueLinkData>     mLinks;
    std::vector<DialogueVariableDef>  mVariables;
    std::vector<DialogueSpeakerDef>   mSpeakers;

    uint32_t mVersion = 1;
};
