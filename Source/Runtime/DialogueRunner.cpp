#include "Runtime/DialogueRunner.hxx"

#include "Assets/DialogueAsset.hxx"
#include "Runtime/DialogueVariableStore.hxx"

#include "Stream.h"

namespace
{
    // Compare two DialogueValues with type promotion: numeric types compare
    // numerically (bool->int->float), strings compare textually. Returns
    // -1 / 0 / +1; for incompatible types returns 0 and sets *outIncompatible.
    int CompareValues(const DialogueValue& a, const DialogueValue& b, bool* outIncompatible)
    {
        if (outIncompatible) *outIncompatible = false;

        // String compare if either side is a string.
        if (a.mType == DialogueValueType::String || b.mType == DialogueValueType::String)
        {
            const std::string& as = a.mString;
            const std::string& bs = b.mString;
            if (a.mType != DialogueValueType::String || b.mType != DialogueValueType::String)
            {
                if (outIncompatible) *outIncompatible = true;
                return 0;
            }
            if (as < bs) return -1;
            if (as > bs) return 1;
            return 0;
        }

        // Numeric compare: promote bool->int->float.
        auto AsDouble = [](const DialogueValue& v) -> double
        {
            switch (v.mType)
            {
                case DialogueValueType::Bool:  return v.mBool ? 1.0 : 0.0;
                case DialogueValueType::Int:   return (double)v.mInt;
                case DialogueValueType::Float: return (double)v.mFloat;
                default:                       return 0.0;
            }
        };
        double da = AsDouble(a);
        double db = AsDouble(b);
        if (da < db) return -1;
        if (da > db) return 1;
        return 0;
    }
}

void DialogueRunner::SetDialogueAsset(DialogueAsset* asset)
{
    if (mRunning)
    {
        Stop();
    }
    mAsset = asset;
}

const DialogueNodeData* DialogueRunner::GetCurrentNode() const
{
    if (mAsset == nullptr || mCurrentNodeId.empty()) return nullptr;
    return mAsset->FindNode(mCurrentNodeId);
}

DialogueValue DialogueRunner::ResolveVariable(const std::string& name) const
{
    if (mVariableStore != nullptr)
    {
        if (auto* v = mVariableStore->GetValue(name)) return *v;
    }
    if (mGlobalStore != nullptr && mGlobalStore != mVariableStore)
    {
        if (auto* v = mGlobalStore->GetValue(name)) return *v;
    }
    return DialogueValue{};
}

bool DialogueRunner::EvaluateCondition(const DialogueConditionData& cond) const
{
    bool exists = false;
    if (mVariableStore && mVariableStore->HasValue(cond.mVariableName)) exists = true;
    if (!exists && mGlobalStore && mGlobalStore->HasValue(cond.mVariableName)) exists = true;

    switch (cond.mOp)
    {
        case DialogueConditionOp::Exists:    return exists;
        case DialogueConditionOp::NotExists: return !exists;
        default: break;
    }
    if (!exists)
    {
        // Comparison against a non-existent variable: treat as false. Avoids
        // surprise truthiness for default-zero values.
        return false;
    }

    DialogueValue lhs = ResolveVariable(cond.mVariableName);
    bool incompatible = false;
    int cmp = CompareValues(lhs, cond.mValue, &incompatible);
    if (incompatible) return false;

    switch (cond.mOp)
    {
        case DialogueConditionOp::Equals:         return cmp == 0;
        case DialogueConditionOp::NotEquals:      return cmp != 0;
        case DialogueConditionOp::Greater:        return cmp > 0;
        case DialogueConditionOp::GreaterOrEqual: return cmp >= 0;
        case DialogueConditionOp::Less:           return cmp < 0;
        case DialogueConditionOp::LessOrEqual:    return cmp <= 0;
        default: return false;
    }
}

bool DialogueRunner::ConditionsPass(const std::vector<DialogueConditionData>& conditions) const
{
    // AND semantics. Empty list = always pass.
    for (const auto& c : conditions)
    {
        if (!EvaluateCondition(c)) return false;
    }
    return true;
}

void DialogueRunner::Start()
{
    if (mAsset == nullptr)
    {
        if (mCallbacks.OnError) mCallbacks.OnError("DialogueRunner::Start: no asset bound");
        return;
    }
    StartAtNode(mAsset->GetStartNodeId());
}

void DialogueRunner::StartAtNode(const std::string& nodeId)
{
    if (mAsset == nullptr)
    {
        if (mCallbacks.OnError) mCallbacks.OnError("DialogueRunner::StartAtNode: no asset bound");
        return;
    }

    if (mAsset->FindNode(nodeId) == nullptr)
    {
        if (mCallbacks.OnError) mCallbacks.OnError("DialogueRunner::StartAtNode: unknown node id '" + nodeId + "'");
        return;
    }

    // Seed defaults so conditions referencing declared variables start with
    // sensible values rather than always failing the existence check.
    if (mVariableStore != nullptr)
    {
        mVariableStore->ApplyDefaults(mAsset->GetGraph());
    }

    mCurrentNodeId = nodeId;
    mRunning = true;
    if (mCallbacks.OnStarted) mCallbacks.OnStarted();
    Advance();
}

void DialogueRunner::Stop()
{
    if (!mRunning) return;
    mRunning = false;
    mCurrentNodeId.clear();
    if (mCallbacks.OnStopped) mCallbacks.OnStopped();
}

void DialogueRunner::ExecuteSideEffects(const DialogueNodeData& node)
{
    // Variable op (if any). Stored against the per-runner variable store; if
    // the variable was previously set in the global store, the per-runner
    // value will shadow it.
    if (node.mVariableOp.mOp != DialogueVarOp::None && mVariableStore != nullptr)
    {
        const std::string& name = node.mVariableOp.mVariableName;
        const DialogueValue& v  = node.mVariableOp.mValue;
        switch (node.mVariableOp.mOp)
        {
            case DialogueVarOp::Set:
                mVariableStore->SetValue(name, v);
                break;
            case DialogueVarOp::Add:
            {
                if (v.mType == DialogueValueType::Float)
                {
                    mVariableStore->SetFloat(name, mVariableStore->GetFloat(name, 0.0f) + v.mFloat);
                }
                else
                {
                    mVariableStore->SetInt(name, mVariableStore->GetInt(name, 0) + v.mInt);
                }
                break;
            }
            case DialogueVarOp::Sub:
            {
                if (v.mType == DialogueValueType::Float)
                {
                    mVariableStore->SetFloat(name, mVariableStore->GetFloat(name, 0.0f) - v.mFloat);
                }
                else
                {
                    mVariableStore->SetInt(name, mVariableStore->GetInt(name, 0) - v.mInt);
                }
                break;
            }
            case DialogueVarOp::Toggle:
                mVariableStore->SetBool(name, !mVariableStore->GetBool(name, false));
                break;
            default: break;
        }
    }

    // Event fire. Surfaced through the OnEvent callback so the embedding node
    // can re-emit it as a Polyphase signal and Lua listeners can react.
    if (!node.mEventName.empty() && node.mType == DialogueNodeType::Event)
    {
        if (mCallbacks.OnEvent) mCallbacks.OnEvent(node.mEventName);
    }
}

void DialogueRunner::Advance()
{
    // Walk the graph, executing side-effect-only nodes until we land on a
    // node that needs user input (Line / Choice with available choices) or
    // we hit End / dead-end.
    constexpr int kMaxHops = 1024;  // safety against infinite jump loops in malformed graphs
    int hops = 0;

    while (mRunning && hops < kMaxHops)
    {
        ++hops;

        const DialogueNodeData* node = GetCurrentNode();
        if (node == nullptr)
        {
            if (mCallbacks.OnError) mCallbacks.OnError("DialogueRunner::Advance: invalid node id '" + mCurrentNodeId + "'");
            mRunning = false;
            if (mCallbacks.OnStopped) mCallbacks.OnStopped();
            return;
        }

        // End nodes terminate the conversation.
        if (node->mType == DialogueNodeType::End)
        {
            mRunning = false;
            mCurrentNodeId.clear();
            if (mCallbacks.OnFinished) mCallbacks.OnFinished();
            return;
        }

        // Side-effect nodes: apply, then jump and continue walking.
        if (node->mType == DialogueNodeType::Event)
        {
            ExecuteSideEffects(*node);
            if (!node->mJumpTargetId.empty())
            {
                mCurrentNodeId = node->mJumpTargetId;
                continue;
            }
            // No jump: settle here so caller can call Continue() — but Event
            // nodes without a jump are unusual; treat as End.
            mRunning = false;
            mCurrentNodeId.clear();
            if (mCallbacks.OnFinished) mCallbacks.OnFinished();
            return;
        }

        if (node->mType == DialogueNodeType::SetVariable)
        {
            ExecuteSideEffects(*node);
            mCurrentNodeId = node->mJumpTargetId;
            if (mCurrentNodeId.empty())
            {
                mRunning = false;
                if (mCallbacks.OnFinished) mCallbacks.OnFinished();
                return;
            }
            continue;
        }

        if (node->mType == DialogueNodeType::Jump)
        {
            mCurrentNodeId = node->mJumpTargetId;
            if (mCurrentNodeId.empty())
            {
                mRunning = false;
                if (mCallbacks.OnFinished) mCallbacks.OnFinished();
                return;
            }
            continue;
        }

        if (node->mType == DialogueNodeType::Branch)
        {
            // Branch: walk choices in declared order, take the first whose
            // conditions pass. If none pass, fall through to jumpTargetId or End.
            std::string nextId;
            for (const auto& c : node->mChoices)
            {
                if (ConditionsPass(c.mConditions))
                {
                    nextId = c.mTargetNodeId;
                    break;
                }
            }
            if (nextId.empty()) nextId = node->mJumpTargetId;
            if (nextId.empty())
            {
                mRunning = false;
                mCurrentNodeId.clear();
                if (mCallbacks.OnFinished) mCallbacks.OnFinished();
                return;
            }
            mCurrentNodeId = nextId;
            continue;
        }

        // Line / Choice nodes settle here for user input.
        ExecuteSideEffects(*node);
        if (mCallbacks.OnLineChanged) mCallbacks.OnLineChanged(*node);
        const auto choices = GetAvailableChoices();
        if (mCallbacks.OnChoicesChanged) mCallbacks.OnChoicesChanged((uint32_t)choices.size());
        return;
    }

    if (hops >= kMaxHops)
    {
        if (mCallbacks.OnError) mCallbacks.OnError("DialogueRunner::Advance: hop limit exceeded (probable jump loop)");
        mRunning = false;
        if (mCallbacks.OnStopped) mCallbacks.OnStopped();
    }
}

std::vector<DialogueChoiceData> DialogueRunner::GetAvailableChoices() const
{
    std::vector<DialogueChoiceData> out;
    const DialogueNodeData* node = GetCurrentNode();
    if (node == nullptr) return out;
    for (const auto& c : node->mChoices)
    {
        if (ConditionsPass(c.mConditions)) out.push_back(c);
    }
    return out;
}

bool DialogueRunner::Continue()
{
    if (!mRunning) return false;
    const DialogueNodeData* node = GetCurrentNode();
    if (node == nullptr) return false;

    // If choices are available, the caller must Choose() — Continue is a no-op.
    if (!node->mChoices.empty())
    {
        const auto choices = GetAvailableChoices();
        if (!choices.empty()) return false;
        // No choices passed conditions — fall through to jumpTargetId.
    }

    if (!node->mJumpTargetId.empty())
    {
        mCurrentNodeId = node->mJumpTargetId;
        Advance();
        return true;
    }

    // No jump: treat as natural End.
    mRunning = false;
    mCurrentNodeId.clear();
    if (mCallbacks.OnFinished) mCallbacks.OnFinished();
    return true;
}

bool DialogueRunner::Choose(uint32_t choiceIndex)
{
    if (!mRunning) return false;
    const auto choices = GetAvailableChoices();
    if (choiceIndex >= choices.size()) return false;
    const DialogueChoiceData& c = choices[choiceIndex];

    if (mCallbacks.OnChoiceSelected) mCallbacks.OnChoiceSelected(c);

    if (c.mTargetNodeId.empty())
    {
        mRunning = false;
        mCurrentNodeId.clear();
        if (mCallbacks.OnFinished) mCallbacks.OnFinished();
        return true;
    }

    mCurrentNodeId = c.mTargetNodeId;
    Advance();
    return true;
}

bool DialogueRunner::ChooseById(const std::string& choiceId)
{
    if (!mRunning) return false;
    const auto choices = GetAvailableChoices();
    for (uint32_t i = 0; i < choices.size(); ++i)
    {
        if (choices[i].mId == choiceId) return Choose(i);
    }
    return false;
}

void DialogueRunner::SaveStream(Stream& stream) const
{
    stream.WriteBool(mRunning);
    stream.WriteString(mCurrentNodeId);
}

void DialogueRunner::LoadStream(Stream& stream)
{
    mRunning = stream.ReadBool();
    stream.ReadString(mCurrentNodeId);
}
