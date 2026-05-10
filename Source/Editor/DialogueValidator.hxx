#pragma once

#include "Assets/DialogueTypes.hxx"

#include <string>
#include <vector>

namespace DialogueAddon
{
    enum class DialogueWarningSeverity : uint8_t
    {
        Info,
        Warning,
        Error
    };

    struct DialogueWarning
    {
        DialogueWarningSeverity mSeverity = DialogueWarningSeverity::Warning;
        std::string             mNodeId;     // empty = graph-level
        std::string             mMessage;
    };

    // Pure utility — analyses a graph and produces a flat list of issues.
    // No engine dependency beyond DialogueTypes; safe to invoke from editor
    // panels and the preview window.
    //
    // Detects (per the spec's validation list):
    //   - missing start node
    //   - duplicate node ids
    //   - choice/jump targets that don't resolve to a node
    //   - missing speaker references
    //   - condition variables not declared in graph.mVariables
    //   - end nodes unreachable (best-effort BFS)
    std::vector<DialogueWarning> ValidateDialogueGraph(const DialogueGraphData& graph);
}
