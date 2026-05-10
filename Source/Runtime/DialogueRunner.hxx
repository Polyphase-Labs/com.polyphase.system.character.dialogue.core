#pragma once

#include "Assets/DialogueTypes.hxx"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class DialogueAsset;
class DialogueVariableStore;
class Stream;

// Pure runtime conversation driver. Holds no Node ownership; the embedding
// DialogueRunner3D node is responsible for the asset reference's lifetime
// and for emitting signals when the runner's state changes.
//
// This class is deliberately exception-free and RTTI-free so it ships to
// 3DS / Wii / GameCube unchanged.
class DialogueRunner
{
public:
    // Lightweight observer hooks. The DialogueRunner3D node fills these in to
    // translate runner state changes into Polyphase signals; editor preview
    // uses them to repaint its window. Set to nullptr (default) to ignore.
    struct Callbacks
    {
        std::function<void(const DialogueNodeData&)>          OnLineChanged;
        std::function<void(uint32_t numChoices)>              OnChoicesChanged;
        std::function<void(const DialogueChoiceData&)>        OnChoiceSelected;
        std::function<void(const std::string& eventName)>     OnEvent;
        std::function<void()>                                  OnStarted;
        std::function<void()>                                  OnStopped;
        std::function<void()>                                  OnFinished;
        std::function<void(const std::string& message)>       OnError;
    };

    void SetDialogueAsset(DialogueAsset* asset);
    DialogueAsset* GetDialogueAsset() const { return mAsset; }

    void SetVariableStore(DialogueVariableStore* store) { mVariableStore = store; }
    DialogueVariableStore* GetVariableStore() const     { return mVariableStore; }

    // The same store can act as both source and side-effect target; in that
    // case set both pointers to the same instance.
    void SetGlobalVariableStore(DialogueVariableStore* store) { mGlobalStore = store; }

    void SetCallbacks(Callbacks cb) { mCallbacks = std::move(cb); }

    // Lifecycle
    void Start();
    void StartAtNode(const std::string& nodeId);
    void Stop();

    bool IsRunning() const { return mRunning; }

    // Current line / available choices.
    const DialogueNodeData*           GetCurrentNode() const;
    std::vector<DialogueChoiceData>   GetAvailableChoices() const;
    const std::string&                GetCurrentNodeId() const { return mCurrentNodeId; }

    // Advance: pick the natural next node (jumpTargetId, or stop if absent or
    // the node has unresolved choices). Returns false if there's no next node
    // (i.e. the conversation is finished).
    bool Continue();

    // Pick a choice by index (in the *available* list — choices filtered out
    // by failed conditions are skipped). Returns false if the index is out of
    // range or the current node isn't a Line/Choice with choices.
    bool Choose(uint32_t choiceIndex);

    // Pick a choice by its declared id. Returns false if no such choice is
    // available on the current node.
    bool ChooseById(const std::string& choiceId);

    // Save / load the live runner state to a stream. Variable store is NOT
    // serialized here — it's owned by the embedder and serialized separately
    // so the global store and per-runner store can be saved/restored to
    // different slots if desired.
    void SaveStream(Stream& stream) const;
    void LoadStream(Stream& stream);

private:
    // Apply a node's side effects: variable op, fire event signal. Does NOT
    // emit OnLineChanged / OnChoicesChanged — that's done at the end of
    // Advance() once the runner has settled on a line-or-choice node.
    void ExecuteSideEffects(const DialogueNodeData& node);

    // Walk the graph from mCurrentNodeId, executing side-effect-only nodes
    // (Event, SetVariable, Branch with passing condition, Jump) until we land
    // on a Line/Choice/End node. Emits OnLineChanged / OnChoicesChanged /
    // OnFinished accordingly.
    void Advance();

    bool ConditionsPass(const std::vector<DialogueConditionData>& conditions) const;
    bool EvaluateCondition(const DialogueConditionData& cond) const;

    DialogueValue ResolveVariable(const std::string& name) const;

    DialogueAsset*         mAsset         = nullptr;
    DialogueVariableStore* mVariableStore = nullptr;
    DialogueVariableStore* mGlobalStore   = nullptr;

    Callbacks   mCallbacks;
    std::string mCurrentNodeId;
    bool        mRunning = false;
};
