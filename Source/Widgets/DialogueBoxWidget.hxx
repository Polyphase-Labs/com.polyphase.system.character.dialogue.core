#pragma once

#include "Nodes/Widgets/Widget.h"
#include "SmartPointer.h"

#include <cstdint>
#include <string>
#include <vector>

class DialogueRunner3D;
class Text;
class Button;

// DialogueBoxWidget — auto-binds to a DialogueRunner3D and renders the
// current line / choices via user-wired child Widgets.
//
// Scene setup (from the editor):
//   - Speaker        : Text widget (name from Speaker Child property)
//   - Body           : Text widget (name from Body Child property)
//   - Choices        : an *array* of Widget references the user assigns in
//                      the inspector. Each entry is one choice slot. When
//                      the runner has N choices, the first N entries are
//                      shown with the choice text and the rest are hidden.
//                      If an entry is a Button, the widget auto-wires its
//                      Activated signal to runner:ChooseDialogueOption(i).
//                      If it's not a Button, the widget sets text on a Text
//                      descendant (preferring a child named "Label", else
//                      the first Text in the subtree) and the user is
//                      responsible for wiring activation themselves.
//
//   - Choices Text   : optional Text-widget name for legacy numbered-list
//                      mode. If the Choices array is empty AND a Text child
//                      with this name exists, the widget renders choices as
//                      "1. ...\n2. ..." and reads keyboard 1-9 in Tick().
//
// No spawning, no cloning. Visibility on each entry is fully controlled by
// the widget — the user just wires the references once.
class DialogueBoxWidget : public Widget
{
public:

    DECLARE_NODE(DialogueBoxWidget, Widget);

    DialogueBoxWidget();
    virtual ~DialogueBoxWidget();

    virtual const char* GetTypeName() const override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;

    virtual void Create() override;
    virtual void Destroy() override;
    virtual void Start() override;
    virtual void Tick(float deltaTime) override;

    // Programmatic binding (alternative to Runner Path). Disconnects from the
    // previous runner and reconnects.
    void SetRunner(DialogueRunner3D* runner);
    DialogueRunner3D* GetRunner() const { return mRunner; }

    void SetTypewriterSpeed(float charsPerSec) { mTypewriterCharsPerSec = charsPerSec; }
    void SetAdvanceKey(int32_t keyCode)        { mAdvanceKey = keyCode; }

    static bool HandlePropChange(Datum* datum, uint32_t index, const void* newValue);

protected:

    void   ConnectRunner(DialogueRunner3D* runner);
    void   DisconnectRunner();

    Text*  FindTextChild(const std::string& name);

    void   ApplyLineToWidgets(const std::string& speakerId, const std::string& body);

    // Resolve & apply the current portrait. Called from OnLineChangedHandler.
    // Looks up the texture asset by name via the engine API, sets it on the
    // Portrait Quad, and toggles visibility based on whether a portrait was
    // resolved.
    void   ApplyPortrait();

    // Drive the "press A to continue" indicator. Active only when the
    // dialogue is running, the typewriter has finished, AND no choices are
    // pending. Toggles visibility on a fixed period so it flashes.
    void   UpdateContinueIndicator(float deltaTime);
    void   HideContinueIndicator();

    // Choice rendering dispatcher. Picks array mode or text mode based on
    // which property the user wired.
    void   RebuildChoices();
    void   RebuildChoicesText();
    void   RefreshChoiceWidgets();    // array mode: show/hide + set labels
    void   HideAllChoiceWidgets();    // array mode: hide every entry

    // Connect / disconnect the per-Button "Activated" handler. Called once
    // each at Start / Destroy.
    void   ConnectChoiceWidgetSignals();
    void   DisconnectChoiceWidgetSignals();

    // Static signal handlers. SignalHandlerFP is a C-style void(Node*, args)
    // pointer; the first parameter is the listener node we passed to
    // ConnectSignal — for our own connections that's `this`, so we downcast.
    static void OnLineChangedHandler   (Node* listener, const std::vector<Datum>& args);
    static void OnChoicesChangedHandler(Node* listener, const std::vector<Datum>& args);
    static void OnDialogueStartedHandler (Node* listener, const std::vector<Datum>& args);
    static void OnDialogueFinishedHandler(Node* listener, const std::vector<Datum>& args);
    // Fired when a wired-up choice Button is activated (mouse click, gamepad,
    // keyboard activate). args[0] is the button as a Node*.
    static void OnChoiceActivatedHandler(Node* listener, const std::vector<Datum>& args);

    // Properties
    std::string mRunnerPath              = "Runner";
    std::string mSpeakerChildName        = "Speaker";
    std::string mBodyChildName           = "Body";
    std::string mChoicesTextChildName    = "Choices";
    float       mTypewriterCharsPerSec   = 40.0f;  // 0 = instant
    int32_t     mAdvanceKey              = 32;     // KEY_SPACE; user can change in editor

    // The wired array of choice slot widgets. Vector storage type is
    // NodePtrWeak per Polyphase property convention for node references.
    std::vector<NodePtrWeak> mChoiceWidgets;

    // Optional Quad widget that displays the current speaker's portrait.
    // Each line's portrait is resolved as: per-node `portrait` if set,
    // else the speaker's `portrait` default. Named expressions (e.g.
    // `Guard_Happy`, `Guard_Angry`) are just distinct texture asset names.
    // If the resolved name is empty (or the asset can't be loaded), the
    // Quad is hidden.
    NodePtrWeak              mPortraitQuad;

    // Optional widget that flashes when the dialogue is waiting for the
    // player to press the advance input on a no-choice line (the typical
    // little arrow/triangle/blinking quad in JRPGs). Hidden during the
    // typewriter reveal, while choices are shown, and when the conversation
    // isn't running. Flash period is full on->off cycle in seconds.
    NodePtrWeak              mContinueIndicator;
    float                    mContinueIndicatorPeriod = 0.6f;

    // Transient state
    DialogueRunner3D*   mRunner               = nullptr;
    bool                mConnected            = false;
    std::string         mFullBodyText;
    std::string         mPendingSpeakerId;
    float               mTypewriterTimer      = 0.0f;
    uint32_t            mTypewriterRevealed   = 0;
    bool                mTypewriterDone       = true;
    bool                mFirstLineDispatched  = false;
    bool                mChoiceSignalsConnected = false;
    float               mContinueIndicatorTimer = 0.0f;
    bool                mContinueIndicatorOn    = false;

    // Cached child pointers refreshed at Start.
    Text*   mSpeakerText      = nullptr;
    Text*   mBodyText         = nullptr;
    Text*   mChoicesText      = nullptr;       // optional, for fallback numbered-list mode
};
