#pragma once

#include "Asset.h"
#include "Assets/DialogueTypes.hxx"

#include <cstdint>
#include <string>
#include <vector>

// Defined in an addon DLL; deliberately not POLYPHASE_API (which would mark the
// class dllimport from Polyphase.dll on Windows). Same convention as VideoClip /
// VideoPlayer3D.
class DialogueAsset : public Asset
{
public:

    DECLARE_ASSET(DialogueAsset, Asset);

    DialogueAsset();
    ~DialogueAsset();

    virtual void LoadStream(Stream& stream, Platform platform) override;
    virtual void SaveStream(Stream& stream, Platform platform) override;
    virtual void Create() override;
    virtual void Destroy() override;
    virtual bool Import(const std::string& path, ImportOptions* options) override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;
    virtual glm::vec4 GetTypeColor() override;
    virtual const char* GetTypeName() override;
    virtual const char* GetTypeImportExt() override;

    // Direct graph access. The runtime DialogueRunner reads this; the editor
    // graph viewer reads & writes it.
    const DialogueGraphData& GetGraph() const          { return mGraph; }
    DialogueGraphData&       GetMutableGraph()         { return mGraph; }
    void                     SetGraph(DialogueGraphData graph) { mGraph = std::move(graph); }

    // Convenience: number of nodes / start id.
    uint32_t           GetNodeCount() const            { return (uint32_t)mGraph.mNodes.size(); }
    const std::string& GetStartNodeId() const          { return mGraph.mStartNodeId; }

    // Find a node by id. Returns nullptr if not present. O(N) linear scan; the
    // runtime caches the start node and any choice targets at Start() so this
    // isn't on the hot path during typical playback.
    const DialogueNodeData* FindNode(const std::string& id) const;

protected:
    DialogueGraphData mGraph;
};
