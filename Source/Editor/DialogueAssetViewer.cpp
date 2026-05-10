// Editor-only — ImGui isn't available on console targets (Wii / GCN / 3DS).
// DialogueCoreAddon.cpp already wraps every reference to this TU in
// #if EDITOR, so guarding the body here keeps the file out of console builds
// without any other change.
#if EDITOR

#include "Editor/DialogueAssetViewer.hxx"
#include "Editor/DialogueValidator.hxx"

#include "Assets/DialogueAsset.hxx"
#include "EngineAPIAccess.hxx"

#include "Plugins/PolyphaseEngineAPI.h"
#include "Plugins/EditorUIHooks.h"

#include "imgui.h"

#include <cstdio>

namespace DialogueAddon
{
    void DialogueAssetInspector(void* nodePtr, void* /*userData*/)
    {
        DialogueAsset* asset = static_cast<DialogueAsset*>(nodePtr);
        if (asset == nullptr) return;

        const DialogueGraphData& g = asset->GetGraph();

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "Dialogue Summary");
        ImGui::BulletText("Start: %s", g.mStartNodeId.c_str());
        ImGui::BulletText("Nodes: %u", (unsigned)g.mNodes.size());
        ImGui::BulletText("Speakers: %u", (unsigned)g.mSpeakers.size());
        ImGui::BulletText("Variables: %u", (unsigned)g.mVariables.size());

        if (ImGui::Button("Open Dialogue Preview"))
        {
            // Set a string flag the preview window picks up next frame.
            // We can't reach across to the preview window directly without a
            // shared state container, so we just open the window via the
            // editor UI hooks API.
            PolyphaseEngineAPI* api = DialogueAddon::GetEngineAPI();
            if (api != nullptr && api->editorUI != nullptr)
            {
                // The window id was registered in DialoguePreviewWindow.cpp.
                api->editorUI->OpenWindow("polyphase.dialogue.preview");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Run Validation"))
        {
            auto warnings = ValidateDialogueGraph(g);
            PolyphaseEngineAPI* api = DialogueAddon::GetEngineAPI();
            if (api != nullptr && api->LogDebug != nullptr)
            {
                if (warnings.empty())
                {
                    api->LogDebug("DialogueAsset '%s': validation passed (0 issues)",
                                  asset->GetName().c_str());
                }
                else
                {
                    for (const auto& w : warnings)
                    {
                        const char* sev = "info";
                        if (w.mSeverity == DialogueWarningSeverity::Warning) sev = "warn";
                        else if (w.mSeverity == DialogueWarningSeverity::Error) sev = "ERROR";
                        if (w.mNodeId.empty())
                        {
                            api->LogWarning("[Dialogue %s] %s", sev, w.mMessage.c_str());
                        }
                        else
                        {
                            api->LogWarning("[Dialogue %s][%s] %s", sev, w.mNodeId.c_str(), w.mMessage.c_str());
                        }
                    }
                }
            }
        }

        // Inline validation summary (collapsing list).
        if (ImGui::CollapsingHeader("Validation"))
        {
            auto warnings = ValidateDialogueGraph(g);
            if (warnings.empty())
            {
                ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "No issues.");
            }
            else
            {
                for (const auto& w : warnings)
                {
                    ImVec4 col(0.7f, 0.7f, 0.7f, 1.0f);
                    if (w.mSeverity == DialogueWarningSeverity::Warning) col = ImVec4(1.0f, 0.85f, 0.3f, 1.0f);
                    else if (w.mSeverity == DialogueWarningSeverity::Error)   col = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                    if (w.mNodeId.empty())
                    {
                        ImGui::TextColored(col, "%s", w.mMessage.c_str());
                    }
                    else
                    {
                        ImGui::TextColored(col, "[%s] %s", w.mNodeId.c_str(), w.mMessage.c_str());
                    }
                }
            }
        }
    }
}

#endif // EDITOR
