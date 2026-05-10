#pragma once

#include "Assets/DialogueTypes.hxx"

#include <string>

// Minimal JSON read/write for DialogueGraphData. Hand-rolled parser (no
// rapidjson, no third-party dep) so the addon stays console-safe and
// exception-free. The schema is fixed and small — see Docs/02-DialogueAssets.md.

namespace DialogueAddon
{
    // Parse a UTF-8 JSON byte buffer into a DialogueGraphData. On failure
    // returns false with a human-readable message in outError; outGraph is left
    // in an unspecified-but-valid state and should not be used.
    bool ParseDialogueJson(const char* data, size_t size,
                           DialogueGraphData& outGraph,
                           std::string& outError);

    // Read a UTF-8 JSON file from disk into a DialogueGraphData. Returns false
    // on read or parse failure with the message in outError.
    bool ReadDialogueJsonFile(const std::string& path,
                              DialogueGraphData& outGraph,
                              std::string& outError);

    // Serialize a DialogueGraphData to a pretty-printed UTF-8 JSON string.
    // Round-trips through ParseDialogueJson byte-for-byte for graphs that did
    // not contain string values with embedded control characters beyond \n,
    // \r, \t, \b, \f, ", \\.
    std::string WriteDialogueJson(const DialogueGraphData& graph);
}
