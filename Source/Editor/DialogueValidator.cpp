// Editor-only — referenced by DialogueAssetViewer.cpp (also editor-only) and
// not pulled in by any runtime path. DialogueCoreAddon.cpp wraps editor
// includes in #if EDITOR.
#if EDITOR

#include "Editor/DialogueValidator.hxx"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace DialogueAddon
{
    namespace
    {
        DialogueWarning MakeError(const std::string& nodeId, const std::string& msg)
        {
            DialogueWarning w;
            w.mSeverity = DialogueWarningSeverity::Error;
            w.mNodeId = nodeId;
            w.mMessage = msg;
            return w;
        }
        DialogueWarning MakeWarn(const std::string& nodeId, const std::string& msg)
        {
            DialogueWarning w;
            w.mSeverity = DialogueWarningSeverity::Warning;
            w.mNodeId = nodeId;
            w.mMessage = msg;
            return w;
        }
        DialogueWarning MakeInfo(const std::string& nodeId, const std::string& msg)
        {
            DialogueWarning w;
            w.mSeverity = DialogueWarningSeverity::Info;
            w.mNodeId = nodeId;
            w.mMessage = msg;
            return w;
        }
    }

    std::vector<DialogueWarning> ValidateDialogueGraph(const DialogueGraphData& graph)
    {
        std::vector<DialogueWarning> out;

        if (graph.mNodes.empty())
        {
            out.push_back(MakeError("", "graph has no nodes"));
            return out;
        }

        if (graph.mStartNodeId.empty())
        {
            out.push_back(MakeError("", "missing 'start' node id"));
        }

        // Index nodes by id for O(1) lookups; also detect duplicates.
        std::unordered_map<std::string, const DialogueNodeData*> byId;
        std::unordered_set<std::string> dupSeen;
        for (const auto& n : graph.mNodes)
        {
            if (n.mId.empty())
            {
                out.push_back(MakeError("", "node with empty id"));
                continue;
            }
            auto it = byId.find(n.mId);
            if (it != byId.end())
            {
                if (dupSeen.find(n.mId) == dupSeen.end())
                {
                    out.push_back(MakeError(n.mId, "duplicate node id"));
                    dupSeen.insert(n.mId);
                }
                continue;
            }
            byId.emplace(n.mId, &n);
        }

        if (!graph.mStartNodeId.empty() && byId.find(graph.mStartNodeId) == byId.end())
        {
            out.push_back(MakeError("", "'start' references unknown node id '" + graph.mStartNodeId + "'"));
        }

        // Speaker / variable name sets.
        std::unordered_set<std::string> speakers;
        for (const auto& s : graph.mSpeakers) speakers.insert(s.mId);
        std::unordered_set<std::string> varNames;
        for (const auto& v : graph.mVariables) varNames.insert(v.mName);

        // Per-node checks.
        for (const auto& n : graph.mNodes)
        {
            if (!n.mSpeakerId.empty() && speakers.find(n.mSpeakerId) == speakers.end())
            {
                out.push_back(MakeWarn(n.mId, "speaker '" + n.mSpeakerId + "' not declared in graph.speakers"));
            }
            if (!n.mJumpTargetId.empty() && byId.find(n.mJumpTargetId) == byId.end())
            {
                out.push_back(MakeError(n.mId, "jumpTargetId '" + n.mJumpTargetId + "' is not a valid node"));
            }
            for (const auto& c : n.mChoices)
            {
                if (c.mTargetNodeId.empty())
                {
                    out.push_back(MakeWarn(n.mId, "choice '" + c.mId + "' has empty target"));
                }
                else if (byId.find(c.mTargetNodeId) == byId.end())
                {
                    out.push_back(MakeError(n.mId, "choice '" + c.mId + "' target '" + c.mTargetNodeId + "' is not a valid node"));
                }
                for (const auto& cond : c.mConditions)
                {
                    if (!cond.mVariableName.empty() && varNames.find(cond.mVariableName) == varNames.end())
                    {
                        out.push_back(MakeWarn(n.mId, "choice condition references undeclared variable '" + cond.mVariableName + "'"));
                    }
                }
            }
            for (const auto& cond : n.mConditions)
            {
                if (!cond.mVariableName.empty() && varNames.find(cond.mVariableName) == varNames.end())
                {
                    out.push_back(MakeWarn(n.mId, "condition references undeclared variable '" + cond.mVariableName + "'"));
                }
            }
            if (n.mVariableOp.mOp != DialogueVarOp::None && !n.mVariableOp.mVariableName.empty())
            {
                if (varNames.find(n.mVariableOp.mVariableName) == varNames.end())
                {
                    out.push_back(MakeInfo(n.mId, "variable op writes to undeclared variable '" + n.mVariableOp.mVariableName + "' (will be created at runtime)"));
                }
            }
        }

        // Reachability (BFS from start). Marks orphan nodes.
        if (!graph.mStartNodeId.empty() && byId.find(graph.mStartNodeId) != byId.end())
        {
            std::unordered_set<std::string> reached;
            std::vector<std::string> queue;
            queue.push_back(graph.mStartNodeId);
            reached.insert(graph.mStartNodeId);
            while (!queue.empty())
            {
                std::string id = queue.back(); queue.pop_back();
                auto it = byId.find(id);
                if (it == byId.end()) continue;
                const DialogueNodeData* n = it->second;
                auto Visit = [&](const std::string& target)
                {
                    if (target.empty()) return;
                    if (reached.insert(target).second) queue.push_back(target);
                };
                Visit(n->mJumpTargetId);
                for (const auto& c : n->mChoices) Visit(c.mTargetNodeId);
            }
            bool hasReachableEnd = false;
            for (const auto& id : reached)
            {
                auto it = byId.find(id);
                if (it != byId.end() && it->second->mType == DialogueNodeType::End)
                {
                    hasReachableEnd = true;
                    break;
                }
            }
            if (!hasReachableEnd)
            {
                out.push_back(MakeWarn("", "no End node is reachable from start (may loop forever)"));
            }
            for (const auto& n : graph.mNodes)
            {
                if (reached.find(n.mId) == reached.end())
                {
                    out.push_back(MakeInfo(n.mId, "node is not reachable from start"));
                }
            }
        }

        return out;
    }
}

#endif // EDITOR
