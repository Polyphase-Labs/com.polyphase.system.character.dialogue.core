#pragma once

// Inspector extension for DialogueAsset. Adds a "Validate" / "Open Preview"
// button row to the standard property grid. Editor-only — guarded by the
// caller via #if EDITOR.

namespace DialogueAddon
{
    // Inspector callback signature: void(void* node, void* userData). The
    // engine passes the selected DialogueAsset* through `node`.
    void DialogueAssetInspector(void* node, void* userData);
}
