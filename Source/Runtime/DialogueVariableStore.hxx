#pragma once

#include "Assets/DialogueTypes.hxx"

#include <cstdint>
#include <string>
#include <unordered_map>

class Stream;

// Runtime variable state. Held by each DialogueRunner3D instance for its
// per-conversation variables; an addon-level singleton in DialogueManager
// holds the optional global store for cross-conversation flags.
//
// Save/load uses the engine's Stream so per-runner state can be serialized
// alongside the rest of the scene save data.
class DialogueVariableStore
{
public:
    void Clear();

    // Replace contents with the defaults declared in a graph. Existing values
    // for variables not in the graph are left untouched (so partial graph
    // imports don't blow away game-wide flags).
    void ApplyDefaults(const DialogueGraphData& graph);

    bool HasValue(const std::string& name) const;

    void SetBool(const std::string& name, bool value);
    void SetInt(const std::string& name, int32_t value);
    void SetFloat(const std::string& name, float value);
    void SetString(const std::string& name, const std::string& value);

    // Generic — preserves the typed slot the value was stored in.
    void SetValue(const std::string& name, const DialogueValue& value);

    bool        GetBool  (const std::string& name, bool defaultValue = false) const;
    int32_t     GetInt   (const std::string& name, int32_t defaultValue = 0) const;
    float       GetFloat (const std::string& name, float defaultValue = 0.0f) const;
    std::string GetString(const std::string& name, const std::string& defaultValue = std::string()) const;

    // Returns nullptr if the variable isn't set.
    const DialogueValue* GetValue(const std::string& name) const;

    void SaveStream(Stream& stream) const;
    void LoadStream(Stream& stream);

    // Iterate (used by the editor preview window). Mutating during iteration
    // is undefined.
    const std::unordered_map<std::string, DialogueValue>& GetAll() const { return mValues; }

private:
    std::unordered_map<std::string, DialogueValue> mValues;
};
