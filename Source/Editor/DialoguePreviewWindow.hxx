#pragma once

// Editor-only interactive preview window for DialogueAsset. Lets a developer
// step through a conversation without putting a DialogueRunner3D in a scene.
//
// Run via Tools -> Dialogue -> Open Preview, or the inspector's "Open Dialogue
// Preview" button.

namespace DialogueAddon
{
    // Window draw callback (signature: void(void* userData)).
    void DialoguePreviewDraw(void* userData);

    // Reset the preview's internal runner state. Called from OnUnload so any
    // cached transient pointers don't outlive the addon DLL.
    void ResetDialoguePreview();
}
