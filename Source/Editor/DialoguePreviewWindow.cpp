// Editor-only — ImGui isn't available on console targets (Wii / GCN / 3DS).
// DialogueCoreAddon.cpp wraps every reference to this TU in #if EDITOR.
#if EDITOR

#include "Editor/DialoguePreviewWindow.hxx"

#include "Assets/DialogueAsset.hxx"
#include "Runtime/DialogueRunner.hxx"
#include "Runtime/DialogueVariableStore.hxx"
#include "EngineAPIAccess.hxx"

#include "AssetManager.h"
#include "Plugins/PolyphaseEngineAPI.h"

#include "imgui.h"

#include <cstdio>
#include <string>
#include <vector>

namespace DialogueAddon
{
    namespace
    {
        // Single transient runner instance. Reset on OnUnload so no Lua refs
        // or AssetRefs survive a DLL reload.
        struct PreviewState
        {
            DialogueAsset*        mAsset = nullptr;
            DialogueVariableStore mStore;
            DialogueRunner        mRunner;
            std::string           mLastSpeaker;
            std::string           mLastBody;
            std::vector<std::string> mEventLog;
            char                  mAssetNameBuf[128] = {0};
            bool                  mWiredCallbacks = false;
        };

        PreviewState& State()
        {
            static PreviewState s;
            return s;
        }

        void WireCallbacks()
        {
            PreviewState& s = State();
            if (s.mWiredCallbacks) return;

            DialogueRunner::Callbacks cb;
            cb.OnLineChanged = [](const DialogueNodeData& n)
            {
                State().mLastSpeaker = n.mSpeakerId;
                State().mLastBody    = n.mText;
            };
            cb.OnChoicesChanged = [](uint32_t /*n*/) {};
            cb.OnChoiceSelected = [](const DialogueChoiceData& /*c*/) {};
            cb.OnEvent = [](const std::string& name)
            {
                State().mEventLog.push_back("event: " + name);
                if (State().mEventLog.size() > 50) State().mEventLog.erase(State().mEventLog.begin());
            };
            cb.OnStarted = []()
            {
                State().mEventLog.push_back("-- started --");
            };
            cb.OnStopped = []() {};
            cb.OnFinished = []()
            {
                State().mEventLog.push_back("-- finished --");
                State().mLastSpeaker.clear();
                State().mLastBody.clear();
            };
            cb.OnError = [](const std::string& msg)
            {
                State().mEventLog.push_back("ERROR: " + msg);
            };

            s.mRunner.SetCallbacks(std::move(cb));
            s.mRunner.SetVariableStore(&s.mStore);
            s.mWiredCallbacks = true;
        }

        DialogueAsset* TryLoadAsset(const char* name)
        {
            if (name == nullptr || name[0] == 0) return nullptr;
            PolyphaseEngineAPI* api = DialogueAddon::GetEngineAPI();
            if (api == nullptr || api->LoadAsset == nullptr) return nullptr;
            Asset* a = api->LoadAsset(name);
            if (a == nullptr) return nullptr;
            if (a->GetType() != DialogueAsset::GetStaticType()) return nullptr;
            return static_cast<DialogueAsset*>(a);
        }
    }

    void ResetDialoguePreview()
    {
        PreviewState& s = State();
        s.mRunner.Stop();
        s.mRunner.SetDialogueAsset(nullptr);
        s.mAsset = nullptr;
        s.mStore.Clear();
        s.mLastSpeaker.clear();
        s.mLastBody.clear();
        s.mEventLog.clear();
        s.mAssetNameBuf[0] = 0;
        s.mWiredCallbacks = false;
    }

    void DialoguePreviewDraw(void* /*userData*/)
    {
        PreviewState& s = State();
        WireCallbacks();

        ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "Dialogue Preview");
        ImGui::Separator();

        ImGui::SetNextItemWidth(280.0f);
        ImGui::InputText("Asset name", s.mAssetNameBuf, sizeof(s.mAssetNameBuf));
        ImGui::SameLine();
        if (ImGui::Button("Load"))
        {
            DialogueAsset* a = TryLoadAsset(s.mAssetNameBuf);
            if (a != nullptr)
            {
                s.mAsset = a;
                s.mRunner.SetDialogueAsset(a);
                s.mEventLog.push_back(std::string("loaded: ") + a->GetName());
            }
            else
            {
                s.mEventLog.push_back(std::string("could not load: ") + s.mAssetNameBuf);
            }
        }

        if (s.mAsset == nullptr)
        {
            ImGui::TextDisabled("Type a DialogueAsset name above and click Load.");
            return;
        }

        ImGui::Text("Loaded: %s", s.mAsset->GetName().c_str());
        ImGui::Text("Start: %s    Nodes: %u",
                    s.mAsset->GetStartNodeId().c_str(),
                    (unsigned)s.mAsset->GetGraph().mNodes.size());

        ImGui::Separator();

        if (!s.mRunner.IsRunning())
        {
            if (ImGui::Button("Start"))
            {
                s.mStore.Clear();
                s.mRunner.Start();
            }
            ImGui::SameLine();
        }
        else
        {
            if (ImGui::Button("Stop"))
            {
                s.mRunner.Stop();
            }
            ImGui::SameLine();
        }
        if (ImGui::Button("Reset"))
        {
            s.mRunner.Stop();
            s.mStore.Clear();
            s.mLastSpeaker.clear();
            s.mLastBody.clear();
            s.mEventLog.clear();
        }

        ImGui::Separator();

        if (s.mRunner.IsRunning())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%s",
                               s.mLastSpeaker.empty() ? "(no speaker)" : s.mLastSpeaker.c_str());
            ImGui::TextWrapped("%s", s.mLastBody.c_str());

            const auto choices = s.mRunner.GetAvailableChoices();
            if (!choices.empty())
            {
                ImGui::Separator();
                for (size_t i = 0; i < choices.size(); ++i)
                {
                    char btn[256];
                    std::snprintf(btn, sizeof(btn), "%u. %s##choice%zu",
                                  (unsigned)(i + 1), choices[i].mText.c_str(), i);
                    if (ImGui::Button(btn))
                    {
                        s.mRunner.Choose((uint32_t)i);
                        break;
                    }
                }
            }
            else
            {
                if (ImGui::Button("Continue"))
                {
                    s.mRunner.Continue();
                }
            }
        }
        else
        {
            ImGui::TextDisabled("Conversation not running. Press Start.");
        }

        ImGui::Separator();
        if (ImGui::CollapsingHeader("Variables"))
        {
            for (const auto& kv : s.mStore.GetAll())
            {
                const DialogueValue& v = kv.second;
                switch (v.mType)
                {
                    case DialogueValueType::Bool:
                        ImGui::BulletText("%s = %s (bool)", kv.first.c_str(), v.mBool ? "true" : "false");
                        break;
                    case DialogueValueType::Int:
                        ImGui::BulletText("%s = %d (int)", kv.first.c_str(), v.mInt);
                        break;
                    case DialogueValueType::Float:
                        ImGui::BulletText("%s = %g (float)", kv.first.c_str(), (double)v.mFloat);
                        break;
                    case DialogueValueType::String:
                        ImGui::BulletText("%s = \"%s\" (string)", kv.first.c_str(), v.mString.c_str());
                        break;
                    default: break;
                }
            }
        }

        if (ImGui::CollapsingHeader("Event Log"))
        {
            for (const auto& line : s.mEventLog)
            {
                ImGui::Text("%s", line.c_str());
            }
        }
    }
}

#endif // EDITOR
