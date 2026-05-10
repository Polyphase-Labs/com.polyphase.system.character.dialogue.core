#include "Widgets/DialogueBoxWidget.hxx"

#include "Nodes/DialogueRunner3D.hxx"
#include "EngineAPIAccess.hxx"

#include "Nodes/Widgets/Text.h"
#include "Nodes/Widgets/Button.h"
#include "Nodes/Widgets/Quad.h"
#include "Assets/Texture.h"

#include "Engine.h"
#include "Log.h"
#include "Datum.h"
#include "Property.h"
#include "World.h"
#include "Plugins/PolyphaseEngineAPI.h"

#include <cstdio>
#include <cstring>

FORCE_LINK_DEF(DialogueBoxWidget);
DEFINE_NODE(DialogueBoxWidget, Widget);

// ---- helpers ---------------------------------------------------------------

namespace
{
    // Class-name string compare via Node::GetClassName (from DECLARE_FACTORY).
    // Reliable across the addon/engine DLL boundary; TypeId equality isn't.
    bool IsClass(const Node* n, const char* name)
    {
        if (n == nullptr) return false;
        const char* cn = n->GetClassName();
        return cn != nullptr && std::strcmp(cn, name) == 0;
    }

    // Find a Text descendant for setting the choice label. Prefers a child
    // named "Label", otherwise returns the first Text in the subtree.
    Text* FindLabelText(Node* root)
    {
        if (root == nullptr) return nullptr;
        if (Node* named = root->FindChild("Label", /*recurse*/ true))
        {
            if (IsClass(named, "Text")) return static_cast<Text*>(named);
        }
        if (IsClass(root, "Text")) return static_cast<Text*>(root);
        for (int32_t i = 0; i < root->GetNumChildren(); ++i)
        {
            if (Text* t = FindLabelText(root->GetChild(i))) return t;
        }
        return nullptr;
    }

    // Set the visible label on a choice slot. Button gets SetTextString
    // (so its internal Text gets updated and the engine's button-state-color
    // logic still works). Anything else falls back to a Text descendant.
    void SetSlotLabel(Node* slot, const std::string& text)
    {
        if (slot == nullptr) return;
        if (IsClass(slot, "Button"))
        {
            static_cast<Button*>(slot)->SetTextString(text);
            return;
        }
        if (Text* label = FindLabelText(slot))
        {
            label->SetText(text);
        }
    }
}

// ---- ctor / dtor / typename ------------------------------------------------

DialogueBoxWidget::DialogueBoxWidget()
{
    mName = "DialogueBox";
}

DialogueBoxWidget::~DialogueBoxWidget()
{
}

const char* DialogueBoxWidget::GetTypeName() const
{
    return "DialogueBoxWidget";
}

// ---- properties ------------------------------------------------------------

bool DialogueBoxWidget::HandlePropChange(Datum* datum, uint32_t /*index*/, const void* newValue)
{
    Property* prop = static_cast<Property*>(datum);
    DialogueBoxWidget* self = static_cast<DialogueBoxWidget*>(prop->mOwner);
    if (self == nullptr) return false;

    if (prop->mName == "Runner Path")
    {
        const std::string* s = reinterpret_cast<const std::string*>(newValue);
        self->mRunnerPath = *s;
        if (self->mConnected)
        {
            self->DisconnectRunner();
            self->Start();
        }
        return true;
    }
    return false;
}

void DialogueBoxWidget::GatherProperties(std::vector<Property>& outProps)
{
    Widget::GatherProperties(outProps);

    SCOPED_CATEGORY("Dialogue Box");

    outProps.push_back(Property(DatumType::String, "Runner Path",       this, &mRunnerPath, 1, HandlePropChange));
    outProps.push_back(Property(DatumType::String, "Speaker Child",     this, &mSpeakerChildName));
    outProps.push_back(Property(DatumType::String, "Body Child",        this, &mBodyChildName));
    outProps.push_back(Property(DatumType::String, "Choices Text Child",this, &mChoicesTextChildName));
    outProps.push_back(Property(DatumType::Float,  "Typewriter Speed",  this, &mTypewriterCharsPerSec));
    outProps.push_back(Property(DatumType::Integer,"Advance Key",       this, &mAdvanceKey));

    // Array of choice-slot widgets, user-wired in the editor. Each entry
    // becomes one choice button. Empty array = use Choices Text Child fallback.
    //
    // DatumType::Node, not Widget: the engine's vector-resize path in
    // Property.cpp PreSet only has a case for DatumType::Node — Widget falls
    // through to default with no mData.n setup, leaving uninitialized memory
    // and asserting in WeakPtr::Clear during scene-load. Single Node-ref
    // properties have the same issue (compare Button::mNavUp which uses Node
    // type even though it's filtering for Buttons).
    outProps.push_back(Property(DatumType::Node, "Choices", this, &mChoiceWidgets).MakeVector());

    // Optional portrait Quad. When set, the widget loads the current line's
    // portrait texture by name (per-node override falls through to speaker
    // default) and applies it via Quad::SetTexture. Named expressions are
    // just distinct asset names — e.g. `Guard_Default`, `Guard_Happy`.
    outProps.push_back(Property(DatumType::Node, "Portrait Quad", this, &mPortraitQuad));

    // Optional "press to continue" indicator. The widget flashes its
    // visibility on/off (period in seconds) only when waiting on advance
    // input — i.e. typewriter done AND no choices pending AND running.
    outProps.push_back(Property(DatumType::Node,  "Continue Indicator",        this, &mContinueIndicator));
    outProps.push_back(Property(DatumType::Float, "Continue Indicator Period", this, &mContinueIndicatorPeriod));
}

// ---- lifecycle -------------------------------------------------------------

void DialogueBoxWidget::Create()
{
    Widget::Create();
}

void DialogueBoxWidget::Destroy()
{
    DisconnectChoiceWidgetSignals();
    DisconnectRunner();
    Widget::Destroy();
}

Text* DialogueBoxWidget::FindTextChild(const std::string& name)
{
    if (name.empty()) return nullptr;
    Node* n = FindChild(name, /*recurse*/ true);
    if (n == nullptr) return nullptr;
    if (!IsClass(n, "Text")) return nullptr;
    return static_cast<Text*>(n);
}

void DialogueBoxWidget::Start()
{
    Widget::Start();

    mSpeakerText = FindTextChild(mSpeakerChildName);
    mBodyText    = FindTextChild(mBodyChildName);
    mChoicesText = FindTextChild(mChoicesTextChildName);

    if (mSpeakerText == nullptr)
    {
        LogWarning("DialogueBoxWidget '%s': no Text child named '%s' (Speaker Child).",
                   mName.c_str(), mSpeakerChildName.c_str());
    }
    if (mBodyText == nullptr)
    {
        LogWarning("DialogueBoxWidget '%s': no Text child named '%s' (Body Child).",
                   mName.c_str(), mBodyChildName.c_str());
    }
    if (mChoiceWidgets.empty() && mChoicesText == nullptr)
    {
        LogWarning("DialogueBoxWidget '%s': no choice rendering set up. Wire the "
                   "Choices array to slot widgets in the editor, or add a Text "
                   "child named '%s' for numbered-list fallback mode.",
                   mName.c_str(), mChoicesTextChildName.c_str());
    }

    // Hide all wired choice slots until the runner produces choices. Also
    // wire each Button slot's Activated signal so clicks/gamepad-A advance
    // the conversation.
    HideAllChoiceWidgets();
    ConnectChoiceWidgetSignals();

    // Resolve the runner. Three strategies, most robust first:
    //   1. World-wide lookup by name.
    //   2. Descendant search from this widget.
    //   3. Ancestor walk looking for a DialogueRunner3D among each ancestor's
    //      direct children.
    DialogueRunner3D* found = nullptr;
    if (!mRunnerPath.empty())
    {
        World* w = GetWorld();
        if (w != nullptr)
        {
            Node* candidate = w->FindNode(mRunnerPath);
            if (candidate != nullptr && IsClass(candidate, "DialogueRunner3D"))
            {
                found = static_cast<DialogueRunner3D*>(candidate);
            }
        }
    }
    if (found == nullptr)
    {
        Node* candidate = FindChild(mRunnerPath, /*recurse*/ true);
        if (candidate != nullptr && IsClass(candidate, "DialogueRunner3D"))
        {
            found = static_cast<DialogueRunner3D*>(candidate);
        }
    }
    if (found == nullptr)
    {
        Node* p = GetParent();
        while (p != nullptr && found == nullptr)
        {
            for (int32_t i = 0; i < p->GetNumChildren(); ++i)
            {
                Node* c = p->GetChild(i);
                if (c != nullptr && IsClass(c, "DialogueRunner3D"))
                {
                    found = static_cast<DialogueRunner3D*>(c);
                    break;
                }
            }
            p = p->GetParent();
        }
    }

    if (found == nullptr)
    {
        LogWarning("DialogueBoxWidget '%s': no DialogueRunner3D found via Runner Path '%s'.",
                   mName.c_str(), mRunnerPath.c_str());
        return;
    }

    ConnectRunner(found);
    if (mRunner != nullptr && mRunner->IsDialogueRunning())
    {
        const std::string& speaker = mRunner->GetCurrentSpeakerName().empty()
            ? mRunner->GetCurrentSpeaker()
            : mRunner->GetCurrentSpeakerName();
        ApplyLineToWidgets(speaker, mRunner->GetCurrentText());
        ApplyPortrait();
        RebuildChoices();
    }
}

// ---- runner connection -----------------------------------------------------

void DialogueBoxWidget::SetRunner(DialogueRunner3D* runner)
{
    if (runner == mRunner) return;
    DisconnectRunner();
    if (runner != nullptr) ConnectRunner(runner);
}

void DialogueBoxWidget::ConnectRunner(DialogueRunner3D* runner)
{
    if (runner == nullptr) return;
    mRunner = runner;
    runner->ConnectSignal("OnLineChanged",       this, &OnLineChangedHandler);
    runner->ConnectSignal("OnChoicesChanged",    this, &OnChoicesChangedHandler);
    runner->ConnectSignal("OnDialogueStarted",   this, &OnDialogueStartedHandler);
    runner->ConnectSignal("OnDialogueFinished",  this, &OnDialogueFinishedHandler);
    mConnected = true;
}

void DialogueBoxWidget::DisconnectRunner()
{
    if (!mConnected || mRunner == nullptr) { mRunner = nullptr; mConnected = false; return; }
    mRunner->DisconnectSignal("OnLineChanged",      this);
    mRunner->DisconnectSignal("OnChoicesChanged",   this);
    mRunner->DisconnectSignal("OnDialogueStarted",  this);
    mRunner->DisconnectSignal("OnDialogueFinished", this);
    mRunner = nullptr;
    mConnected = false;
}

// ---- line + choices --------------------------------------------------------

void DialogueBoxWidget::ApplyLineToWidgets(const std::string& speakerId, const std::string& body)
{
    mPendingSpeakerId = speakerId;
    mFullBodyText = body;

    mTypewriterRevealed = 0;
    mTypewriterTimer = 0.0f;
    mTypewriterDone = (mTypewriterCharsPerSec <= 0.0f);

    if (mSpeakerText != nullptr) mSpeakerText->SetText(speakerId);
    if (mBodyText != nullptr)
    {
        mBodyText->SetText(mTypewriterDone ? mFullBodyText : std::string());
        if (mTypewriterDone) mTypewriterRevealed = (uint32_t)mFullBodyText.size();
    }
}

void DialogueBoxWidget::RebuildChoices()
{
    if (!mChoiceWidgets.empty()) RefreshChoiceWidgets();
    else                         RebuildChoicesText();
}

void DialogueBoxWidget::RebuildChoicesText()
{
    if (mChoicesText == nullptr || mRunner == nullptr) return;
    uint32_t n = mRunner->GetNumChoices();
    std::string s;
    for (uint32_t i = 0; i < n; ++i)
    {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%u. ", (unsigned)(i + 1));
        s.append(buf);
        s.append(mRunner->GetChoiceText(i));
        if (i + 1 < n) s.push_back('\n');
    }
    mChoicesText->SetText(s);
}

void DialogueBoxWidget::HideAllChoiceWidgets()
{
    // Clear the global selected pointer if it's one of ours — Button input
    // doesn't gate on visibility, so a hidden-but-selected Button still
    // activates on A press and fires ChooseDialogueOption with a stale index.
    Button* selected = Button::GetSelectedButton();
    for (auto& weak : mChoiceWidgets)
    {
        Node* n = weak.Get();
        if (n == nullptr) continue;
        if (selected != nullptr && static_cast<Node*>(selected) == n)
        {
            Button::SetSelectedButton(nullptr);
            selected = nullptr;
        }
        // Also clear nav links so d-pad can't navigate from a still-visible
        // sibling Button into this now-hidden one.
        if (IsClass(n, "Button"))
        {
            Button* btn = static_cast<Button*>(n);
            btn->SetNavUp(nullptr);
            btn->SetNavDown(nullptr);
            btn->SetNavLeft(nullptr);
            btn->SetNavRight(nullptr);
        }
        n->SetVisible(false);
    }
}

void DialogueBoxWidget::RefreshChoiceWidgets()
{
    if (mRunner == nullptr) { HideAllChoiceWidgets(); return; }

    const uint32_t numChoices = mRunner->GetNumChoices();
    const uint32_t slots      = (uint32_t)mChoiceWidgets.size();

    if (numChoices > slots)
    {
        LogWarning("DialogueBoxWidget '%s': runner has %u choices but only %u slots wired. "
                   "Add more entries to the Choices array in the inspector.",
                   mName.c_str(), (unsigned)numChoices, (unsigned)slots);
    }

    // First pass: clear nav links on every wired Button. Without this, a
    // previously-visible button's NavDown could still point at a now-hidden
    // button, letting d-pad-Down land on it and activate it on A press
    // (causing ChooseDialogueOption with a stale, out-of-range index).
    for (uint32_t i = 0; i < slots; ++i)
    {
        Node* slot = mChoiceWidgets[i].Get();
        if (slot != nullptr && IsClass(slot, "Button"))
        {
            Button* btn = static_cast<Button*>(slot);
            btn->SetNavUp(nullptr);
            btn->SetNavDown(nullptr);
            btn->SetNavLeft(nullptr);
            btn->SetNavRight(nullptr);
        }
    }

    // Second pass: show + label the first N slots, hide the rest, wire fresh
    // nav between adjacent visible Buttons.
    Button* firstButton = nullptr;
    Button* prevButton  = nullptr;
    for (uint32_t i = 0; i < slots; ++i)
    {
        Node* slot = mChoiceWidgets[i].Get();
        if (slot == nullptr) continue;

        if (i < numChoices)
        {
            slot->SetVisible(true);
            SetSlotLabel(slot, mRunner->GetChoiceText(i));

            if (IsClass(slot, "Button"))
            {
                Button* btn = static_cast<Button*>(slot);
                if (firstButton == nullptr) firstButton = btn;
                if (prevButton != nullptr)
                {
                    prevButton->SetNavDown(btn);
                    btn->SetNavUp(prevButton);
                }
                prevButton = btn;
            }
        }
        else
        {
            slot->SetVisible(false);
        }
    }

    // Selected-button management: if there are visible buttons, focus the
    // first; otherwise clear the global selected pointer if it's one of
    // ours. Engine Button input doesn't gate on visibility, so a hidden-
    // but-selected Button activates on A press and fires
    // ChooseDialogueOption with an out-of-range index.
    if (firstButton != nullptr)
    {
        Button::SetSelectedButton(firstButton);
    }
    else
    {
        Button* selected = Button::GetSelectedButton();
        if (selected != nullptr)
        {
            for (auto& weak : mChoiceWidgets)
            {
                if (weak.Get() == static_cast<Node*>(selected))
                {
                    Button::SetSelectedButton(nullptr);
                    break;
                }
            }
        }
    }
}

// ---- Activated signal wiring ----------------------------------------------

void DialogueBoxWidget::ConnectChoiceWidgetSignals()
{
    if (mChoiceSignalsConnected) return;
    for (auto& weak : mChoiceWidgets)
    {
        Node* n = weak.Get();
        if (n == nullptr) continue;
        // Only Buttons emit the Activated signal. Non-Button slots: the user
        // wires their own click handler and calls runner:ChooseDialogueOption(i).
        if (IsClass(n, "Button"))
        {
            n->ConnectSignal("Activated", this, &OnChoiceActivatedHandler);
        }
    }
    mChoiceSignalsConnected = true;
}

void DialogueBoxWidget::DisconnectChoiceWidgetSignals()
{
    if (!mChoiceSignalsConnected) return;
    // Clear the global "selected" pointer if it's one of ours.
    Button* selected = Button::GetSelectedButton();
    for (auto& weak : mChoiceWidgets)
    {
        Node* n = weak.Get();
        if (n == nullptr) continue;
        if (selected != nullptr && static_cast<Node*>(selected) == n)
        {
            Button::SetSelectedButton(nullptr);
            selected = nullptr;
        }
        if (IsClass(n, "Button"))
        {
            n->DisconnectSignal("Activated", this);
        }
    }
    mChoiceSignalsConnected = false;
}

// ---- signal handlers -------------------------------------------------------

void DialogueBoxWidget::OnLineChangedHandler(Node* listener, const std::vector<Datum>& args)
{
    DialogueBoxWidget* w = static_cast<DialogueBoxWidget*>(listener);
    if (w == nullptr) return;

    // Prefer the runner's resolved display name + portrait over the raw
    // speaker id from the signal args. The runner's getters fall through
    // graph.mSpeakers and per-node overrides, so they're authoritative.
    std::string speaker;
    std::string text = (args.size() > 1) ? args[1].GetString() : std::string();
    if (w->mRunner != nullptr)
    {
        speaker = w->mRunner->GetCurrentSpeakerName();
    }
    if (speaker.empty() && args.size() > 0)
    {
        speaker = args[0].GetString();
    }

    w->ApplyLineToWidgets(speaker, text);
    w->ApplyPortrait();
    w->mFirstLineDispatched = true;
}

void DialogueBoxWidget::OnChoicesChangedHandler(Node* listener, const std::vector<Datum>& /*args*/)
{
    DialogueBoxWidget* w = static_cast<DialogueBoxWidget*>(listener);
    if (w == nullptr) return;
    w->RebuildChoices();
}

void DialogueBoxWidget::OnDialogueStartedHandler(Node* listener, const std::vector<Datum>& /*args*/)
{
    DialogueBoxWidget* w = static_cast<DialogueBoxWidget*>(listener);
    if (w != nullptr) w->SetVisible(true);
}

void DialogueBoxWidget::OnDialogueFinishedHandler(Node* listener, const std::vector<Datum>& /*args*/)
{
    DialogueBoxWidget* w = static_cast<DialogueBoxWidget*>(listener);
    if (w == nullptr) return;
    if (w->mSpeakerText != nullptr) w->mSpeakerText->SetText("");
    if (w->mBodyText    != nullptr) w->mBodyText->SetText("");
    if (w->mChoicesText != nullptr) w->mChoicesText->SetText("");
    w->HideAllChoiceWidgets();
    if (Node* p = w->mPortraitQuad.Get()) p->SetVisible(false);
    w->HideContinueIndicator();
    w->mFullBodyText.clear();
    w->mTypewriterRevealed = 0;
    w->mTypewriterDone = true;
}

void DialogueBoxWidget::ApplyPortrait()
{
    if (mRunner == nullptr) return;
    Node* portraitNode = mPortraitQuad.Get();
    if (portraitNode == nullptr) return;  // user didn't wire one — no-op

    if (!IsClass(portraitNode, "Quad"))
    {
        // Diagnostic: the wired widget isn't a Quad. We could fall back to
        // setting a Texture child's property, but that's unusual — log once
        // and bail.
        LogWarning("DialogueBoxWidget '%s': Portrait Quad is wired to a '%s', "
                   "expected 'Quad'. Texture won't be applied.",
                   mName.c_str(),
                   portraitNode->GetClassName() ? portraitNode->GetClassName() : "<null>");
        return;
    }
    Quad* quad = static_cast<Quad*>(portraitNode);

    const std::string& portraitName = mRunner->GetCurrentPortrait();
    if (portraitName.empty())
    {
        portraitNode->SetVisible(false);
        return;
    }

    PolyphaseEngineAPI* api = DialogueAddon::GetEngineAPI();
    if (api == nullptr || api->LoadAsset == nullptr) return;

    Asset* asset = api->LoadAsset(portraitName.c_str());
    if (asset == nullptr)
    {
        LogWarning("DialogueBoxWidget '%s': portrait asset '%s' not found",
                   mName.c_str(), portraitName.c_str());
        portraitNode->SetVisible(false);
        return;
    }

    // Asset's GetClassName comes from DECLARE_FACTORY too — works cross-DLL.
    const char* assetClass = asset->GetClassName();
    if (assetClass == nullptr || std::strcmp(assetClass, "Texture") != 0)
    {
        LogWarning("DialogueBoxWidget '%s': portrait asset '%s' is a '%s', "
                   "expected 'Texture'", mName.c_str(), portraitName.c_str(),
                   assetClass ? assetClass : "<null>");
        portraitNode->SetVisible(false);
        return;
    }

    quad->SetTexture(static_cast<Texture*>(asset));
    portraitNode->SetVisible(true);
}

void DialogueBoxWidget::HideContinueIndicator()
{
    if (Node* n = mContinueIndicator.Get())
    {
        n->SetVisible(false);
    }
    mContinueIndicatorOn = false;
    mContinueIndicatorTimer = 0.0f;
}

void DialogueBoxWidget::UpdateContinueIndicator(float deltaTime)
{
    Node* n = mContinueIndicator.Get();
    if (n == nullptr) return;  // user didn't wire one — no-op

    // Active iff: dialogue running, typewriter done, and waiting on a
    // continue-style advance (no choices pending). When choices are visible
    // the player should be picking a choice, not pressing continue, so the
    // indicator stays hidden.
    const bool active = mRunner != nullptr
                        && mRunner->IsDialogueRunning()
                        && mTypewriterDone
                        && mRunner->GetNumChoices() == 0;

    if (!active)
    {
        if (mContinueIndicatorOn || n->IsVisible(false))
        {
            n->SetVisible(false);
        }
        mContinueIndicatorOn = false;
        mContinueIndicatorTimer = 0.0f;
        return;
    }

    // Flash. Period is the full on+off cycle; we toggle every half-period.
    const float halfPeriod = (mContinueIndicatorPeriod > 0.0f)
        ? mContinueIndicatorPeriod * 0.5f
        : 0.3f;
    mContinueIndicatorTimer += deltaTime;
    if (mContinueIndicatorTimer >= halfPeriod)
    {
        mContinueIndicatorTimer -= halfPeriod;
        mContinueIndicatorOn = !mContinueIndicatorOn;
        n->SetVisible(mContinueIndicatorOn);
    }
    else if (!mContinueIndicatorOn && !n->IsVisible(false))
    {
        // Just-activated case: show immediately so the player isn't waiting
        // up to half a period for the first flash.
        mContinueIndicatorOn = true;
        n->SetVisible(true);
    }
}

void DialogueBoxWidget::OnChoiceActivatedHandler(Node* listener, const std::vector<Datum>& args)
{
    DialogueBoxWidget* w = static_cast<DialogueBoxWidget*>(listener);
    if (w == nullptr || w->mRunner == nullptr) return;
    if (args.empty()) return;
    Node* btn = args[0].GetNode().Get();
    if (btn == nullptr) return;
    for (uint32_t i = 0; i < w->mChoiceWidgets.size(); ++i)
    {
        if (w->mChoiceWidgets[i].Get() == btn)
        {
            w->mRunner->ChooseDialogueOption(i);
            return;
        }
    }
}

// ---- Tick ------------------------------------------------------------------

void DialogueBoxWidget::Tick(float deltaTime)
{
    Widget::Tick(deltaTime);

    if (mRunner == nullptr) return;

    // Flash the press-to-continue indicator (or hide it if not waiting).
    UpdateContinueIndicator(deltaTime);

    // Typewriter reveal.
    if (!mTypewriterDone && mBodyText != nullptr && !mFullBodyText.empty() && mTypewriterCharsPerSec > 0.0f)
    {
        mTypewriterTimer += deltaTime;
        const float secPerChar = 1.0f / mTypewriterCharsPerSec;
        while (mTypewriterTimer >= secPerChar && mTypewriterRevealed < mFullBodyText.size())
        {
            mTypewriterTimer -= secPerChar;
            ++mTypewriterRevealed;
        }
        mBodyText->SetText(mFullBodyText.substr(0, mTypewriterRevealed));
        if (mTypewriterRevealed >= mFullBodyText.size()) mTypewriterDone = true;
    }

    // Keyboard fallback for text-mode choices (numbered 1-9). For wired-array
    // mode the engine's Button class handles gamepad A on the focused slot.
    PolyphaseEngineAPI* api = DialogueAddon::GetEngineAPI();
    if (api == nullptr) return;
    auto KeyHit = [&](int32_t key) -> bool
    {
        return api->IsKeyJustPressed != nullptr && api->IsKeyJustPressed(key);
    };

    if (mRunner->GetNumChoices() > 0)
    {
        for (int32_t i = 0; i < 9; ++i)
        {
            if (KeyHit(49 + i))  // '1' .. '9'
            {
                if ((uint32_t)i < mRunner->GetNumChoices())
                {
                    mRunner->ChooseDialogueOption((uint32_t)i);
                    return;
                }
            }
        }
    }
    else if (KeyHit(mAdvanceKey))
    {
        if (!mTypewriterDone)
        {
            mTypewriterDone = true;
            mTypewriterRevealed = (uint32_t)mFullBodyText.size();
            if (mBodyText != nullptr) mBodyText->SetText(mFullBodyText);
        }
        else
        {
            mRunner->ContinueDialogue();
        }
    }
}
